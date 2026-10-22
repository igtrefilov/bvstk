// Production UI and real nested bundles, isolated from the hardware.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile} from "node:fs/promises";
import {resolve,extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium}=await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo=resolve(fileURLToPath(new URL("../../",import.meta.url)));
const ids=["pca9545a","pcf8563","p24c02a","pca9555"];
const path=id=>id==="pca9545a"?id:`pca9545a/${id}`;
const profiles=Object.fromEntries(await Promise.all(ids.map(async id=>[
  id,JSON.parse(await readFile(resolve(repo,`configs/i2c/${path(id)}/${id}.json`),"utf8"))
])));
const devices=ids.map(name=>({name,presence:"responding",topology:{
  parent:profiles[name].transport.parent || "",channel:profiles[name].transport.channel || 0,
  channels:profiles[name].protocol.switch?.channels.length || 0,controller:"pl_i2c_0"
}}));
const samples={pca9545a:[[0,1]],pcf8563:[[0,0],[1,0],[2,0x42]],p24c02a:[[0,0xa5]],pca9555:[[0,1],[6,255]]};
const caches=Object.fromEntries(ids.map(id=>[id,samples[id].map(([reg,val])=>({reg,val,valid:true,source:"read",updated_ms:12000}))]));
const calls=[];
const server=http.createServer(async(req,res)=>{
  try {
    const u=new URL(req.url,"http://localhost"),q=Object.fromEntries(u.searchParams);
    calls.push({method:req.method,path:u.pathname,q});
    const json=v=>{res.writeHead(200,{"Content-Type":"application/json"});res.end(JSON.stringify(v));};
    if(u.pathname==="/api/ui/bus") {
      if(!q.name)return json({ready:true,devices});
      assert(profiles[q.name]);
      if(q.action){assert.equal(q.action,"cache");return json({cache:caches[q.name]});}
      const p=profiles[q.name];
      return json({config:{name:p.id,addr_7b:Number(p.transport.address),reg_count:p.resources.length,
        max_value_code:Number(p.protocol.options.max_write_value),policy:"whitelist",whitelist:[],blacklist:[],settings:[]},
        file_name:`${path(p.id)}/${p.id}.json`,register_map:"register_map.json",view:"view.js",
        presence:"responding",cache:caches[p.id],saved:true,revision:"1"});
    }
    if(u.pathname==="/api/ui/file") {
      assert.equal(q.volume,"flash");
      assert(/^\/config\/i2c\/pca9545a\/(?:[a-z0-9]+\/)?(?:register_map.json|view.js)$/.test(q.path));
      const body=await readFile(resolve(repo,"configs",q.path.slice("/config/".length)));
      res.writeHead(200,{"Content-Type":"application/octet-stream"});
      return res.end(body);
    }
    if(u.pathname==="/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head>
      <body><nav id="tree"></nav><main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
      import {busPage} from "/js/buses.js";import {updateDeviceTree} from "/js/device-tree.js";import {bundlePath} from "/js/indicators.js";
      let ctrl;window.bundlePath=bundlePath;window.treeDevices=${JSON.stringify(devices)};
      window.updateTree=()=>updateDeviceTree(document.getElementById("tree"),"i2c",window.treeDevices);
      window.openDevice=async name=>{ctrl?.abort();ctrl=new AbortController();window.page=await busPage(document.getElementById("root"),new URLSearchParams({name,tab:"indicators"}),{signal:ctrl.signal,setContext(){}},"i2c");};
      window.updateTree();await window.openDevice("pca9545a");window.ready=true;
      </script></body></html>`);
    }
    if(u.pathname==="/favicon.ico"){res.writeHead(204);return res.end();}
    const file=resolve(repo,"web/assets",u.pathname.slice(1));
    assert(file.startsWith(resolve(repo,"web/assets")+"/"));
    const body=await readFile(file);
    res.writeHead(200,{"Content-Type":extname(file)===".js"?"text/javascript":"text/css"});res.end(body);
  }catch(e){if(!res.headersSent)res.writeHead(500);res.end(String(e));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME||"/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page=await browser.newPage({viewport:{width:1300,height:950}}),errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);
  const frame=()=>page.frameLocator("iframe.device-indicators-frame");
  await frame().locator('[data-channel="0"] .channel-title').filter({hasText:"Канал 0"}).waitFor();
  assert.equal(await frame().locator(".route-badge").count(),0);
  assert.equal(await frame().locator(".irq-badge").count(),0);
  assert.equal(await page.locator('#tree a[data-device-name]').count(),4);
  assert.equal(await page.locator('[data-branch="pca9545a:0"] a').count(),2);
  assert.equal(await page.locator('[data-branch="pca9545a:3"] a').count(),1);
  assert.equal(await page.locator('.topology-empty').count(),2);
  await page.evaluate(()=>{
    for(const d of document.querySelectorAll("#tree details"))d.open=true;
    const a=document.querySelector('[data-device-name="pcf8563"]');a.focus();a.dataset.preserved="yes";
    window.treeDevices[1].presence="unavailable";window.updateTree();
  });
  assert.equal(await page.locator('[data-device-name="pcf8563"]').getAttribute("data-preserved"),"yes");
  assert.equal(await page.evaluate(()=>document.activeElement.dataset.deviceName),"pcf8563");
  assert.equal(await page.locator("#tree details:not([open])").count(),0);
  for(const id of ids.slice(1)) {
    await page.evaluate(id=>window.openDevice(id),id);
    if(id==="pcf8563")await frame().locator('[data-metric="seconds"] strong').filter({hasText:"42"}).waitFor();
    if(id==="p24c02a")await frame().locator('[data-byte="0"]').filter({hasText:"A5"}).waitFor();
    if(id==="pca9555")await frame().locator('[data-field="config0.p0_0"]').filter({hasText:"Вход"}).waitFor();
    assert.equal(await page.locator("[data-map-message]").innerText(),"");
    assert.equal(await page.locator("[data-decoded-register]").count(),profiles[id].resources.length);
    await page.evaluate(()=>document.querySelector("iframe").dataset.preserved="yes");
    await page.evaluate(()=>window.page.refresh(true));
    assert.equal(await page.locator("iframe").getAttribute("data-preserved"),"yes");
  }
  caches.pca9555.find(e=>e.reg===6).val=0;
  await frame().locator('[data-field="config0.p0_0"]').filter({hasText:"Выход"}).waitFor();
  assert.equal(await page.evaluate(()=>window.bundlePath("i2c","pca9545a/pcf8563/pcf8563.json","view.js")),"/config/i2c/pca9545a/pcf8563/view.js");
  assert.equal(calls.some(c=>c.method!=="GET" || (c.q.action && c.q.action!=="cache")),false);
  assert.deepEqual(errors,[]);
  console.log("I2C topology browser: nested tree, focus/expansion, four real views, nested Flash paths, live cache and no hardware writes passed");
} finally {
  await browser.close();await new Promise(r=>server.close(r));
}
