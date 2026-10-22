// Real bundle scripts in the production card and sandbox. Local HTTP only.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile, readdir, mkdir} from "node:fs/promises";
import {resolve, dirname, basename, extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo = resolve(fileURLToPath(new URL("../../",import.meta.url)));
const profiles = [];
async function discover(dir, bus) {
  for (const entry of await readdir(dir,{withFileTypes:true})) {
    const path = resolve(dir,entry.name);
    if (entry.isDirectory()) await discover(path,bus);
    else if (entry.name === basename(dir) + ".json") {
      const profile = JSON.parse(await readFile(path,"utf8"));
      assert.equal(profile.icon,"icon.js");
      const source = await readFile(resolve(dir,profile.icon),"utf8");
      assert(Buffer.byteLength(source) <= 16384);
      profiles.push({bus,profile,dir,file_name:path.slice(resolve(repo,"configs",bus).length+1)});
    }
  }
}
await discover(resolve(repo,"configs/i2c"),"i2c");
await discover(resolve(repo,"configs/mdio"),"mdio");
assert.equal(profiles.length,9);
const calls = [], faults = {icon:null,name:null,cache:false};
let presence = "responding", uptime = 1500;
const cache = [{reg:0,val:3,valid:true,source:"I2C",updated_ms:1000}];
function detail(item) {
  const p = item.profile;
  return {config:{name:p.id,device:p.device,addr_7b:Number(p.transport.address || "0x50"),
      phy_addr:Number(p.transport.address || "0x01"),reg_count:p.resources.length,max_value_code:255,
      policy:"whitelist",whitelist:[],blacklist:[],settings:[]},
    icon:faults.name ?? p.icon,register_map:p.register_map || "",view:p.view || "",
    file_name:item.file_name,presence,uptime_ms:uptime,cache,revision:"1",saved:true,
    address_spaces:(p.transport.address_spaces || []).map((s,i,list)=>({id:s.id,address:Number(s.address),register_count:s.register_count,first:list.slice(0,i).reduce((n,s)=>n+s.register_count,0)}))};
}
const server = http.createServer(async(req,res)=>{
  try {
    const u = new URL(req.url,"http://localhost"), q = Object.fromEntries(u.searchParams);
    calls.push({method:req.method,path:u.pathname,q});
    assert.equal(req.method,"GET","the icon must never mutate firmware or the bus");
    const json = (data,status=200) => {res.writeHead(status,{"Content-Type":"application/json"});res.end(JSON.stringify(data));};
    if (u.pathname === "/api/ui/bus") {
      if (!q.name) return json({ready:true,devices:profiles.filter(i=>i.bus===q.bus).map(i=>({name:i.profile.id,presence}))});
      const item = profiles.find(i=>i.bus===q.bus && i.profile.id===q.name); assert(item);
      if (q.action) {
        assert.equal(q.action,"cache");
        return faults.cache ? json({error:"offline"},503) : json({cache,presence,uptime_ms:uptime});
      }
      return json(detail(item));
    }
    if (u.pathname === "/api/ui/file") {
      assert.equal(q.volume,"flash");
      assert(profiles.some(i=>q.path === `/config/${i.bus}/${dirname(i.file_name)}/${basename(q.path)}`));
      if (q.path.endsWith("/icon.js") && faults.icon !== null) {
        if (faults.icon === "missing") return json({error:"missing"},404);
        res.writeHead(200,{"Content-Type":"application/octet-stream"}); return res.end(faults.icon);
      }
      const path = resolve(repo,"configs",q.path.slice("/config/".length));
      assert(path.startsWith(resolve(repo,"configs")+"/"));
      res.writeHead(200,{"Content-Type":"application/octet-stream"});return res.end(await readFile(path));
    }
    if (u.pathname === "/gallery") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><link rel="stylesheet" href="/css/app.css"><style>
        body{padding:36px;background:#f2f4f5}h1{font:24px Arial;margin-bottom:28px}.gallery{display:grid;grid-template-columns:repeat(3,1fr);gap:18px}
        article{display:flex;gap:20px;align-items:center;background:white;border:1px solid #d3dce1;padding:24px}h2{margin:0 0 5px;font:18px Arial}p{margin:0;color:#687c8a;font:13px Arial}
      </style></head><body><h1>Пиктограммы устройств bvstk</h1><div class="gallery"></div><script type="module">
        import {deviceIconWorkspace} from '/js/device-icon.js';
        const devices=${JSON.stringify(profiles.map(i=>({bus:i.bus,detail:detail(i)})))};
        for(const item of devices){const card=document.createElement('article');card.innerHTML='<span class="device-icon-slot" role="img"></span><div><h2></h2><p></p></div>';document.querySelector('.gallery').append(card);
          card.querySelector('h2').textContent=item.detail.config.device.model;card.querySelector('p').textContent=item.bus.toUpperCase();
          deviceIconWorkspace(card.querySelector('span'),{...item,signal:new AbortController().signal});}
      </script></body></html>`);
    }
    if (u.pathname === "/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head><body><main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
        import {busPage} from '/js/buses.js';
        let ctrl;window.openDevice=async(bus,name,tab='overview')=>{ctrl?.abort();ctrl=new AbortController();document.getElementById('root').innerHTML='';
          window.page=await busPage(document.getElementById('root'),new URLSearchParams({name,tab}),{signal:ctrl.signal,setContext(){}},bus);};
        window.ready=true;
      </script></body></html>`);
    }
    if (u.pathname === "/favicon.ico") {res.writeHead(204);return res.end();}
    const path = resolve(repo,"web/assets",u.pathname.slice(1));assert(path.startsWith(resolve(repo,"web/assets")+"/"));
    res.writeHead(200,{"Content-Type":extname(path)===".js"?"text/javascript":"text/css"});res.end(await readFile(path));
  } catch(error) {if(!res.headersSent)res.writeHead(500);res.end(String(error));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser = await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page = await browser.newPage({viewport:{width:1200,height:900}}), errors = [];
  page.on("pageerror",e=>errors.push(e.message));
  const origin = `http://127.0.0.1:${server.address().port}`;
  await page.goto(origin);await page.waitForFunction(()=>window.ready);
  const open = async(bus,name,tab) => page.evaluate(args=>window.openDevice(...args),[bus,name,tab]);
  const ready = () => page.locator('[data-device-icon][data-icon-state="ready"]').waitFor();
  const frame = () => page.frameLocator("iframe.device-icon-frame");
  const drawings = new Set();
  for (const item of profiles) {
    await open(item.bus,item.profile.id); await ready();
    assert.equal(await page.locator('iframe.device-icon-frame').getAttribute('sandbox'),"allow-scripts");
    const svg = frame().locator("svg");await svg.waitFor();
    drawings.add(await svg.innerHTML());
    // Include stroke clearance: each mark belongs inside its viewBox.
    assert(await svg.evaluate(el=>[...el.children].every(child=>{
      if(child.tagName==='title')return true;const b=child.getBBox();return b.x>=1 && b.y>=1 && b.x+b.width<=63 && b.y+b.height<=63;
    })),item.profile.id+" drawing is clipped");
    assert.equal(await page.locator('[data-device-icon] > svg').isVisible(),false);
  }
  assert.equal(drawings.size,9);
  // The map-less PHY still has a bundle icon on its Parameters tab.
  await open("mdio","dp83848c","indicators");await ready();
  await page.getByText("Карта регистров не указана в профиле устройства.").waitFor();
  assert.equal(await page.locator("iframe.device-indicators-frame").count(),0);

  const before = calls.filter(c=>c.q.path==="/config/i2c/pca9545a/p24c02a/icon.js").length;
  await open("i2c","p24c02a");await ready();
  await page.locator("iframe.device-icon-frame").evaluate(el=>el.dataset.original="yes");
  await page.locator('[data-tab="registers"]').click();
  presence="unavailable";uptime=2500;
  await frame().locator('#icon-root[data-presence="unavailable"]').waitFor();
  faults.cache=true;await frame().locator('#icon-root[data-stale="true"]').waitFor();
  faults.cache=false;presence="responding";uptime=3000;
  await frame().locator('#icon-root[data-presence="responding"][data-stale="false"]').waitFor();
  await page.locator('[data-tab="indicators"]').click();
  await page.frameLocator("iframe.device-indicators-frame").locator("#known-count").waitFor();
  cache[0].val=17;uptime=4000;
  await frame().locator("#icon-root").evaluate(el=>{
    const update=window.bvstkIcon.update;window.bvstkIcon.update=s=>{update(s);el.dataset.value=String(s.cache[0]?.val);};
  });
  await frame().locator('#icon-root[data-value="17"]').waitFor();
  await page.evaluate(()=>window.page.refresh(true));
  assert.equal(await page.locator("iframe.device-icon-frame").getAttribute("data-original"),"yes");
  assert.equal(calls.filter(c=>c.q.path==="/config/i2c/pca9545a/p24c02a/icon.js").length,before+1);
  await page.setViewportSize({width:390,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));

  for (const source of ["missing","not valid javascript",'window.bvstkIcon={apiVersion:2,mount(){},update(){}};',
    'window.bvstkIcon={apiVersion:1,mount(){},update(){throw Error("broken update")}};'," ".repeat(16385)]) {
    faults.icon=source;await open("i2c","p24c02a");
    await page.locator('[data-device-icon][data-icon-state="fallback"]').waitFor();
    assert.equal(await page.locator('[data-device-icon] > svg').isVisible(),true);
    await page.locator('[data-tab="policy"]').click();
    assert.equal(await page.locator("iframe.device-icon-frame").count(),0);
  }
  faults.icon=`window.bvstkIcon={apiVersion:1,mount(root){try{parent.document.body;}catch{root.textContent='DOM isolated';}fetch('/forbidden',{method:'PUT'}).catch(()=>root.textContent+=' network blocked');},update(){}};`;
  await open("mdio","lan8720");await ready();await frame().getByText("DOM isolated network blocked").waitFor();
  assert(!calls.some(c=>c.path==="/forbidden"));
  faults.icon=null;
  for (const name of ["", "../icon.js", "https://example.com/icon.js", "icon.svg"]) {
    faults.name=name;const count=calls.filter(c=>c.path==="/api/ui/file").length;
    await open("mdio","lan8720");await page.locator('[data-device-icon][data-icon-state="fallback"]').waitFor();
    assert.equal(calls.filter(c=>c.path==="/api/ui/file").length,count);
  }
  faults.name=null;
  await page.setViewportSize({width:1100,height:620});await page.goto(origin+"/gallery");
  await page.waitForFunction(()=>document.querySelectorAll('[data-icon-state="ready"]').length===9);
  if(process.env.BVSTK_WEB_SCREENSHOT){await mkdir(dirname(process.env.BVSTK_WEB_SCREENSHOT),{recursive:true});await page.screenshot({path:process.env.BVSTK_WEB_SCREENSHOT,fullPage:true});}
  assert.equal(errors.length,0,errors.join("\n"));
  console.log("Device icons: 9 real bundles, nested paths, map-less profiles, stable frames, cache updates, fallbacks and sandbox isolation passed");
} finally {await browser.close();await new Promise(r=>server.close(r));}
