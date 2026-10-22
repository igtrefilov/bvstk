// Production bus page, sandbox and switch bundle. In-memory HTTP data only.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile, mkdir} from "node:fs/promises";
import {resolve, extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo = resolve(fileURLToPath(new URL("../../",import.meta.url)));
const bundle = resolve(repo,"configs/i2c/pca9545a");
const profile = JSON.parse(await readFile(resolve(bundle,"pca9545a.json"),"utf8"));
const children = await Promise.all(["pcf8563","p24c02a","sfp","pca9555"].map(async name => {
  const p = JSON.parse(await readFile(resolve(bundle,name,name+".json"),"utf8"));
  return {name,model:p.device.model,label:p.device.label || "",class:p.device.class,
    channel:p.transport.channel,presence:"responding",
    addresses:p.transport.address_spaces?.map(s=>Number(s.address)) || [Number(p.transport.address)]};
}));
let clockMs = 20000, presence = "responding", fail = false, polls = 0;
let cache = [{reg:0,val:255,valid:false,source:"I2C",updated_ms:0}];
let routing = {control_reg:0,select_mask:15,
  channels:profile.protocol.switch.channels.map((c,index)=>({index,select:Number(c.select),match_mask:Number(c.match_mask)})),
  last_read:{reg:0,val:0,updated_ms:19500},children};
const calls = [];
const server = http.createServer(async (req,res) => {
  try {
    const u = new URL(req.url,"http://localhost"), q = Object.fromEntries(u.searchParams);
    calls.push({method:req.method,path:u.pathname,q});
    assert.equal(req.method,"GET","The switch form must not issue writes or physical reads");
    const json = data => {res.writeHead(200,{"Content-Type":"application/json"});res.end(JSON.stringify(data));};
    if(u.pathname==="/api/ui/bus") {
      if(!q.name)return json({ready:true,devices:[{name:"pca9545a",address:112,presence}]});
      assert.equal(q.name,"pca9545a");
      if(q.action) {
        assert.equal(q.action,"cache");polls++;
        if(fail){res.writeHead(503);return res.end("temporary disconnect");}
        return json({cache,uptime_ms:clockMs,presence,routing});
      }
      return json({config:{name:"pca9545a",addr_7b:112,reg_count:1,max_value_code:15,
        policy:"whitelist",whitelist:[],blacklist:[],settings:[]},cache,presence,routing,
        uptime_ms:clockMs,topology:{controller:"pl_i2c_0",parent:"",channel:0,channels:4},
        file_name:"pca9545a/pca9545a.json",register_map:"register_map.json",view:"view.js",icon:"icon.js",
        revision:"1",saved:true});
    }
    if(u.pathname==="/api/ui/file") {
      const asset=q.path.split("/").at(-1);
      assert.equal(q.volume,"flash");assert.equal(q.path,"/config/i2c/pca9545a/"+asset);
      assert(["register_map.json","view.js","icon.js"].includes(asset));
      const body=await readFile(resolve(bundle,asset));assert(body.length<=65536);
      res.writeHead(200,{"Content-Type":"application/octet-stream"});return res.end(body);
    }
    if(u.pathname==="/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head><body><main id="root" style="margin:0;padding:24px"></main><div id="notice" class="hidden"></div><script type="module">
        import {busPage} from '/js/buses.js';
        const ctrl=new AbortController();window.page=await busPage(document.getElementById('root'),new URLSearchParams('name=pca9545a&tab=indicators'),{signal:ctrl.signal,setContext(){}},'i2c');window.ready=true;
      </script></body></html>`);
    }
    if(u.pathname==="/favicon.ico"){res.writeHead(204);return res.end();}
    const path=resolve(repo,"web/assets",u.pathname.slice(1));
    assert(path.startsWith(resolve(repo,"web/assets")+"/"));
    res.writeHead(200,{"Content-Type":extname(path)===".js"?"text/javascript":"text/css"});res.end(await readFile(path));
  }catch(error){if(!res.headersSent)res.writeHead(500);res.end(String(error));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page=await browser.newPage({viewport:{width:1440,height:1100}}), errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>window.ready);
  const frame=()=>page.frameLocator("iframe.device-indicators-frame");
  const settle=()=>page.waitForTimeout(850);
  const screenshot=async name=>{
    if(!process.env.BVSTK_SWITCH_SCREENSHOT_DIR)return;
    await mkdir(process.env.BVSTK_SWITCH_SCREENSHOT_DIR,{recursive:true});
    await page.screenshot({path:resolve(process.env.BVSTK_SWITCH_SCREENSHOT_DIR,name+".png"),fullPage:true});
  };
  assert.equal(await frame().locator(".channel").count(),4);
  assert.equal(await frame().locator("#pause").count(),0);
  assert.doesNotMatch(await page.locator("body").innerText(),/Просмотр не выполняет физических чтений или записей/);
  assert.equal(await frame().locator(".route-badge").count(),0);
  assert.equal(await frame().locator(".irq-badge").count(),0);
  assert.equal(await frame().locator("#register-title").count(),0);
  assert.equal(await frame().locator("#method").count(),0);
  assert.equal(await page.locator("[data-decoded-register]").count(),1);
  assert.deepEqual(await frame().locator(".legend span").allTextContents(),["Готово","Не готово"]);
  assert.equal(await frame().locator(".endpoint").count(),4);
  assert.equal(await frame().locator('[data-channel="0"] .endpoint').count(),2);
  assert.equal(await frame().locator('[data-channel="2"] .empty-channel').innerText(),"Профили не заданы");
  assert.equal(await frame().locator('[data-channel="1"] .offline').count(),0);
  await screenshot("desktop");
  await page.locator("iframe.device-indicators-frame").evaluate(el=>el.dataset.original="yes");
  await frame().locator('[data-channel="1"]').evaluate(el=>el.dataset.original="yes");
  clockMs+=2000;
  await settle();
  assert.equal(await frame().locator('[data-channel="1"]').getAttribute("data-original"),"yes");
  const readyNode=frame().locator('[data-channel="0"] .endpoint').first();
  routing.children[0].presence="unavailable";
  await settle();
  assert.match(await readyNode.getAttribute("title"),/Не готов/);

  clockMs=24000;
  routing.children[2].presence="unavailable";
  const before=polls;await settle();assert(polls>before);
  await page.evaluate(()=>window.page.refresh(true));await settle();
  assert.match(await frame().locator('[data-channel="1"] .endpoint').first().getAttribute("title"),/Не готов/);
  assert.equal(await page.locator("iframe.device-indicators-frame").getAttribute("data-original"),"yes");
  await screenshot("all-bits");

  fail=true;await settle();
  assert.equal(await frame().locator(".endpoint .offline").count(),4);await screenshot("stale");
  fail=false;await settle();
  // Profile changes are data-driven; unsafe labels stay plain text.
  routing.children=[{name:"sensor_x",label:'<img src=x onerror="alert(1)">',class:"generic",presence:"responding",channel:2,addresses:[64]}];
  clockMs=200;cache=[];await settle();
  assert.equal(await frame().locator(".endpoint").count(),1);
  assert.equal(await frame().locator(".endpoint img").count(),0);
  for(const width of [1440,1200,1000,800,600,390]) {
    await page.setViewportSize({width,height:1000});await page.waitForTimeout(100);
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),`Host overflow at ${width}`);
    assert(await frame().locator("body").evaluate(el=>el.scrollWidth<=innerWidth+1),`View overflow at ${width}`);
  }
  routing.children=children;clockMs=500;
  routing.children.forEach(d=>d.presence="responding");await settle();
  await screenshot("mobile");
  // Older firmware: show unknown topology.
  routing=undefined;clockMs=1000;cache=[];await settle();
  assert.equal(await frame().locator(".endpoint").count(),0);
  assert.equal(await frame().locator(".empty-channel").first().innerText(),"Топология недоступна");
  clockMs=2000;await settle();
  assert(calls.every(c=>c.method==="GET" && (!c.q.action || c.q.action==="cache")));
  assert.equal(calls.filter(c=>c.path==="/api/ui/file").length,3,"Do not reload bundle assets during polling");
  assert.deepEqual(errors,[]);
  console.log("PCA9545A UI: topology, device readiness, refresh, profile changes, escaping, responsive layout and zero bus I/O passed");
}finally{await browser.close();await new Promise(r=>server.close(r));}
