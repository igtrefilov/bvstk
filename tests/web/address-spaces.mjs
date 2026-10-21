// One card, independent windows, qualified editors and live decoding.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile} from "node:fs/promises";
import {resolve,extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium}=await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo=resolve(fileURLToPath(new URL("../../",import.meta.url)));
const map=await readFile(resolve(repo,"tests/host/fixtures/device-multi-space_register_map.json"),"utf8");
const spaces=[{id:"identity",address:80,first:0,register_count:4},
  {id:"diagnostics",address:81,first:4,register_count:4}];
const config={name:"dual",addr_7b:80,reg_count:8,max_value_code:255,policy:"whitelist",
  whitelist:[{reg:4,val:18}],blacklist:[],settings:[]};
const cache=Array.from({length:8},(_,reg)=>({reg,val:reg<4?17:34,valid:true,source:"I2C",updated_ms:1000}));
const calls=[];
const server=http.createServer(async(req,res)=>{
  try {
    const u=new URL(req.url,"http://localhost"),q=Object.fromEntries(u.searchParams);
    const json=value=>{res.writeHead(200,{"Content-Type":"application/json"});res.end(JSON.stringify(value));};
    calls.push({method:req.method,path:u.pathname,q});
    if(u.pathname==="/api/ui/bus") {
      if(!q.name)return json({ready:true,devices:[{name:"dual",presence:"responding",address_spaces:spaces}]});
      assert.equal(q.name,"dual");
      if(q.action==="cache")return json({cache});
      if(q.action==="read") {
        assert.equal(req.method,"PUT");assert.equal(Number(q.reg),4);
        return json({ok:true,reg:4,val:cache[4].val});
      }
      assert(!q.action);
      return json({config,cache,address_spaces:spaces,file_name:"switch/dual/dual.json",
        register_map:"register_map.json",view:"view.js",presence:"responding",saved:true,revision:"1"});
    }
    if(u.pathname==="/api/ui/file") {
      assert.equal(q.volume,"flash");
      if(q.path==="/config/i2c/switch/dual/register_map.json") {
        res.writeHead(200,{"Content-Type":"application/json"});return res.end(map);
      }
      assert.equal(q.path,"/config/i2c/switch/dual/view.js");
      res.writeHead(200,{"Content-Type":"text/javascript"});
      return res.end(`window.bvstkView={apiVersion:1,
        mount(root){this.node=document.createElement("p");this.node.id="spaces";root.append(this.node);},
        update(s){this.node.textContent=s.device.address_spaces.map(w=>w.id+":"+w.address+":"+w.first).join(",");}
      };`);
    }
    if(u.pathname==="/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html><head><link rel="stylesheet" href="/css/app.css"></head><body>
        <main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
        import {busPage} from "/js/buses.js";
        window.controller=await busPage(document.getElementById("root"),new URLSearchParams({name:"dual",tab:"registers"}),
          {signal:new AbortController().signal,setContext(){}},"i2c");window.ready=true;
        </script></body></html>`);
    }
    if(u.pathname==="/favicon.ico"){res.writeHead(204);return res.end();}
    const file=resolve(repo,"web/assets",u.pathname.slice(1));
    assert(file.startsWith(resolve(repo,"web/assets")+"/"));
    res.writeHead(200,{"Content-Type":extname(file)===".js"?"text/javascript":"text/css"});
    res.end(await readFile(file));
  } catch(e) {if(!res.headersSent)res.writeHead(500);res.end(String(e));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME||"/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page=await browser.newPage({viewport:{width:1300,height:950}}),errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);
  assert.equal(await page.locator(".device-name").count(),1);
  assert.match(await page.locator(".device-meta").innerText(),/identity 0x50, diagnostics 0x51/);
  assert.equal(await page.locator('[data-register="0"] td').first().innerText(),"identity:0x00");
  assert.equal(await page.locator('[data-register="4"] td').first().innerText(),"diagnostics:0x00");
  await page.locator('[data-register="4"]').click();
  assert.equal(await page.locator("#reg").inputValue(),"diagnostics:0x00");
  assert.equal(await page.locator(".inspector .value").innerText(),"0x22");
  await page.locator("[data-read]").click();
  await page.waitForFunction(()=>document.querySelector("[data-read]")?.disabled===false);
  assert.equal(calls.filter(c=>c.q.action==="read").length,1);
  assert.equal(calls.find(c=>c.q.action==="read").q.reg,"4");
  await page.locator("#reg").fill("0x00");
  assert.equal(await page.locator("[data-read]").isDisabled(),true);
  await page.locator("#reg").fill("identity:0x00");
  await page.locator("#reg").press("Tab");
  assert.equal(await page.locator("#reg").inputValue(),"identity:0x00");
  assert.equal(await page.locator(".inspector .value").innerText(),"0x11");
  await page.locator('[data-tab="policy"]').click();
  assert.equal(await page.locator('[data-rule-field="reg"]').inputValue(),"diagnostics:0x00");
  await page.locator('[data-rule-field="reg"]').fill("identity:0x00");
  assert.match(await page.locator("#rule-title").innerText(),/identity:0x00/);
  assert.equal(await page.locator('[data-rule="0"] td').first().innerText(),"identity:0x00");
  await page.evaluate(()=>window.controller.refresh(true)); // discard browser-only draft
  await page.locator('[data-tab="indicators"]').click();
  await page.waitForFunction(()=>document.querySelector('[data-indicator="identity"] strong')?.textContent==="17");
  await page.locator('article.card > details > summary').click();
  assert.equal(await page.locator('[data-indicator="diagnostics"] strong').innerText(),"34");
  assert.equal(await page.locator('[data-decoded-register="diagnostics_00"] [data-raw]').innerText(),"0x22");
  await page.frameLocator("iframe").locator("#spaces").filter({hasText:"identity:80:0,diagnostics:81:4"}).waitFor();
  cache[4].val=51;
  await page.waitForFunction(()=>document.querySelector('[data-indicator="diagnostics"] strong')?.textContent==="51");
  assert.equal(await page.locator('[data-indicator="identity"] strong').innerText(),"17");
  assert.equal(await page.locator('[data-decoded-register="identity_00"] [data-raw]').innerText(),"0x11");
  assert.equal(calls.some(c=>["write","apply","save"].includes(c.q.action)),false);
  assert.deepEqual(errors,[]);
  console.log("Address-space UI: one card, qualified registers/policies, independent live decoding, nested assets and view metadata passed");
} finally {
  await browser.close();await new Promise(r=>server.close(r));
}
