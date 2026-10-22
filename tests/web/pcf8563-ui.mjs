// Exercise the production page and sandbox with cache-only HTTP responses.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile,mkdir} from "node:fs/promises";
import {resolve,extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium}=await import(process.env.BVSTK_PLAYWRIGHT_MODULE||"playwright");
const repo=resolve(fileURLToPath(new URL("../../",import.meta.url)));
const bundle=resolve(repo,"configs/i2c/pca9545a/pcf8563");
const words=[0,3,0x52,0x38,0x14,0x22,4,0x10,0x26,0x30,7,0x80,0x80,0x80,0x82,45];
let cache=[],clockMs=2200,presence="responding",fail=false,polls=0;
const calls=[],serverErrors=[];
function normal(){
  cache=words.map((val,reg)=>({reg,val,valid:true,source:"I2C",updated_ms:clockMs-200,read_id:1,read_duration_ms:5}));
}
normal();
const server=http.createServer(async(req,res)=>{
  try{
    const u=new URL(req.url,"http://localhost"),q=Object.fromEntries(u.searchParams);
    calls.push({method:req.method,path:u.pathname,q});
    assert.equal(req.method,"GET","Indicators must not write or initiate a bus read");
    const json=data=>{res.writeHead(200,{"Content-Type":"application/json"});res.end(JSON.stringify(data));};
    if(u.pathname==="/api/ui/bus"){
      if(!q.name)return json({ready:true,devices:[{name:"pcf8563",presence,address:81}]});
      assert.equal(q.name,"pcf8563");
      if(q.action){
        assert.equal(q.action,"cache");polls++;
        if(fail){res.writeHead(503);return res.end("disconnected");}
        return json({cache,uptime_ms:clockMs,presence});
      }
      return json({config:{name:"pcf8563",addr_7b:81,reg_count:16,max_value_code:255,
        policy:"whitelist",whitelist:[],blacklist:[],settings:[]},cache,presence,uptime_ms:clockMs,
        topology:{parent:"pca9545a",channel:0,controller:"pl_i2c_0"},
        file_name:"pca9545a/pcf8563/pcf8563.json",register_map:"register_map.json",view:"view.js",icon:"icon.js",
        saved:true,revision:"1"});
    }
    if(u.pathname==="/api/ui/file"){
      const asset=q.path.split("/").at(-1);
      assert.equal(q.volume,"flash");assert.equal(q.path,"/config/i2c/pca9545a/pcf8563/"+asset);
      assert(["register_map.json","view.js","icon.js"].includes(asset));
      const body=await readFile(resolve(bundle,asset));assert(body.length<=65536);
      res.writeHead(200,{"Content-Type":"application/octet-stream"});return res.end(body);
    }
    if(u.pathname==="/"){
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end('<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head><body><main id="root" style="margin:0;padding:24px"></main><div id="notice" class="hidden"></div><script type="module">import {busPage} from "/js/buses.js";window.page=await busPage(document.getElementById("root"),new URLSearchParams("name=pcf8563&tab=indicators"),{signal:new AbortController().signal,setContext(){}},"i2c");window.ready=true;</script></body></html>');
    }
    if(u.pathname==="/favicon.ico"){res.writeHead(204);return res.end();}
    const path=resolve(repo,"web/assets",u.pathname.slice(1));
    assert(path.startsWith(resolve(repo,"web/assets")+"/"));
    res.writeHead(200,{"Content-Type":extname(path)===".js"?"text/javascript":"text/css"});
    res.end(await readFile(path));
  }catch(error){serverErrors.push(String(error));if(!res.headersSent)res.writeHead(500);res.end(String(error));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME||"/usr/bin/google-chrome",args:["--no-sandbox"]});
try{
  const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto("http://127.0.0.1:"+server.address().port+"/");
  await page.waitForFunction(()=>window.ready);
  const frame=page.frameLocator("iframe.device-indicators-frame");
  const waitText=(id,value)=>frame.locator("#"+id).filter({hasText:new RegExp("^"+value+"$")}).waitFor();
  await waitText("coverage-label","16 из 16");
  assert(await frame.locator("#combined-clock").isVisible());
  assert.equal(await frame.locator("#hours").innerText(),"14");
  assert.equal(await frame.locator("#alarm-time").innerText(),"07:30");
  assert.equal(await frame.locator("#timer-count").innerText(),"45");
  assert.equal(await frame.locator("#clkout-frequency").innerText(),"32 768");
  assert.equal(await frame.locator("#scenario").count(),0);
  assert.equal(await frame.locator("#next-sample").count(),0);
  const screenshot=async name=>{
    if(!process.env.BVSTK_RTC_SCREENSHOT_DIR)return;
    await mkdir(process.env.BVSTK_RTC_SCREENSHOT_DIR,{recursive:true});
    const width=page.viewportSize().width;
    const height=await page.evaluate(()=>document.documentElement.scrollHeight);
    await page.setViewportSize({width,height:height+30});
    await page.screenshot({path:resolve(process.env.BVSTK_RTC_SCREENSHOT_DIR,name+".png"),fullPage:true});
    await page.setViewportSize({width,height:1100});
  };
  await screenshot("desktop");
  await frame.locator("[data-register='9']").click();
  await waitText("inspector-value","0x30");
  // The opaque iframe reports its expanded height asynchronously. Wait for
  // the parent layout before clicking a byte below the original frame edge.
  const expanded=await frame.locator("#device-root").evaluate(node=>Math.ceil(node.getBoundingClientRect().height));
  await page.waitForFunction(h=>parseFloat(document.querySelector("iframe.device-indicators-frame").style.height)>=h,expanded);
  await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  await frame.locator("[data-byte='15']").click();
  await waitText("inspector-value","0x2D");
  await frame.locator("[data-byte='15']").evaluate(node=>node.dataset.original="yes");
  // Stable DOM, source selection and keyboard focus across updates.
  cache[15].val=44;clockMs=3200;cache.forEach(e=>e.updated_ms=3000);
  await waitText("inspector-value","0x2C");
  assert.equal(await frame.locator("[data-byte='15']").getAttribute("data-original"),"yes");
  assert(await frame.locator("[data-byte='15']").evaluate(node=>document.activeElement===node));
  await frame.locator("#pause").click();
  const frozen=await frame.locator("#device-root").innerText(),before=polls;
  cache[2].val=0x53;cache[15].val=43;clockMs=4200;cache.forEach(e=>e.updated_ms=4000);
  const deadline=Date.now()+6000;
  while(polls<before+2&&Date.now()<deadline)await new Promise(r=>setTimeout(r,100));
  assert(polls>=before+2);
  assert.equal(await frame.locator("#device-root").innerText(),frozen,"All values and timestamps must freeze");
  await frame.locator("#pause").click();await waitText("seconds","53");await waitText("timer-count","43");
  await frame.locator("#raw-data summary").click();

  cache[2].val|=0x80;await waitText("alert-title","Время требует проверки");
  assert(await frame.locator("#combined-clock").isVisible());
  await screenshot("voltage");
  normal();cache[0].val=0x20;
  await waitText("alert-title","Счёт времени остановлен");
  await waitText("clkout-state","Разрешён");
  cache[13].val=0x81;await waitText("clkout-state","Остановлен");
  normal();cache[1].val=0x1e;
  await waitText("irq-value","Запрос будильника");await waitText("timer-irq","Запрещено");
  cache[1].val=0x15;await waitText("irq-value","Импульсный режим");
  cache[1].val=0x0f;await waitText("irq-value","Оба источника");
  normal();cache[9].val=0x7a;
  await waitText("alarm-state","Ошибка");
  normal();cache[3].val=0x7a;
  await waitText("alert-title","Некорректный календарь");
  assert(!(await frame.locator("#combined-clock").isVisible()));
  normal();cache.forEach(e=>{delete e.read_id;delete e.read_duration_ms;});
  await waitText("alert-title","Календарь получен раздельно");
  assert(!(await frame.locator("#combined-clock").isVisible()));
  await screenshot("separate");
  normal();cache=cache.slice(0,1);
  await waitText("coverage-label","1 из 16");await waitText("validity-value","Достоверность неизвестна");
  normal();cache[2].source="settings";
  await waitText("coverage-label","15 из 16");
  assert.equal(await frame.locator("[data-field='2'] strong").innerText(),"—");

  normal();fail=true;await waitText("alert-title","Нет свежего ответа");
  fail=false;normal();clockMs+=6000;
  await waitText("alert-title","Данные устарели");
  normal();presence="unavailable";await waitText("alert-title","Нет свежего ответа");
  // New PS startup returns an empty cache, not previous calendar data.
  presence="responding";clockMs=50;cache=[];
  await waitText("coverage-label","0 из 16");
  assert(!(await frame.locator("#combined-clock").isVisible()));
  clockMs=2200;normal();await waitText("coverage-label","16 из 16");
  for(const width of [1440,1200,1000,800,600,390]){
    await page.setViewportSize({width,height:1100});
    await page.waitForTimeout(100);
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),"Host overflow at "+width);
    assert(await frame.locator("body").evaluate(node=>node.scrollWidth<=innerWidth+1),"View overflow at "+width);
  }
  await screenshot("mobile");
  assert(calls.every(c=>c.method==="GET"&&(!c.q.action||c.q.action==="cache")));
  assert.equal(calls.filter(c=>c.path==="/api/ui/file").length,3,"Bundle assets are loaded once");
  assert.deepEqual(serverErrors,[]);assert.deepEqual(errors,[]);
  console.log("PCF8563 UI: production sandbox, coherent/partial RTC, flags, cache updates, full pause, stable focus, reboot, mobile and zero bus I/O passed");
}finally{await browser.close();await new Promise(r=>server.close(r));}
