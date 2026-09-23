// Isolated browser fixture: uses production bus page and real device assets.
// No connection to the board, no physical reads/writes.
import http from "node:http";
import {readFile} from "node:fs/promises";
import {resolve,extname} from "node:path";
import {fileURLToPath} from "node:url";
import assert from "node:assert/strict";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo = resolve(fileURLToPath(new URL("../../",import.meta.url)));
const mapText = await readFile(resolve(repo,"configs/mdio/lan8720/register_map.json"),"utf8");
const realView = await readFile(resolve(repo,"configs/mdio/lan8720/view.js"),"utf8");
const axpMap = await readFile(resolve(repo,"configs/i2c/axp15060/register_map.json"),"utf8");
const axpView = await readFile(resolve(repo,"configs/i2c/axp15060/view.js"),"utf8");
let i2cAssets = false;
let viewText = realView, mapMissing = false, cacheFail = false;
const entry = (reg,val) => ({reg,val,valid:true,source:"read",updated_ms:123456});
let cache = [entry(0,0x1000),entry(1,0x782d),entry(2,7),entry(3,0xc0f1),entry(17,2),entry(18,0xe1),entry(26,3),entry(27,0),entry(31,0x1018)];
const calls = [];
const server = http.createServer(async(req,res)=>{
  try {
    const u = new URL(req.url,"http://localhost"), q = Object.fromEntries(u.searchParams);
    calls.push({method:req.method,path:u.pathname,q});
    const reply = (data,status=200) => { res.writeHead(status,{"Content-Type":"application/json"}); res.end(JSON.stringify(data)); };
    if (u.pathname === "/api/ui/bus") {
      const config = {name:q.bus==="mdio"?"lan8720":"axp15060",phy_addr:1,addr_7b:54,reg_count:q.bus==="mdio"?32:74,policy:"whitelist",whitelist:[],blacklist:[],settings:[]};
      if (!q.name) return reply({ready:true,devices:[{name:config.name,presence:"responding"}]});
      if (q.action) {
        assert.equal(q.action,"cache"); assert.equal(req.method,"GET");
        return cacheFail ? reply({error:"Offline"},503) : reply({cache});
      }
      return reply({config,file_name:config.name+"/"+config.name+".json",register_map:q.bus==="mdio"||i2cAssets?"register_map.json":"",view:q.bus==="mdio"||i2cAssets?"view.js":"",presence:"responding",cache,saved:true,revision:"1234"});
    }
    if (u.pathname === "/api/ui/file") {
      assert.equal(q.volume,"flash");
      if (mapMissing) return reply({error:"No file"},404);
      res.writeHead(200,{"Content-Type":"application/octet-stream"});
      if (q.path.startsWith("/config/i2c/axp15060/")) return res.end(q.path.endsWith("register_map.json") ? axpMap : axpView);
      return res.end(q.path.endsWith("register_map.json") ? mapText : viewText);
    }
    if (u.pathname === "/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head><body><main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
      import {busPage} from '/js/buses.js';
      let ctrl; window.openBus=async(bus='mdio')=>{ctrl?.abort();ctrl=new AbortController();window.page=await busPage(document.getElementById('root'),new URLSearchParams('tab=indicators'),{signal:ctrl.signal,setContext(){}},bus);};
      await window.openBus(); window.ready=true;
      </script></body></html>`);
    }
    const path = resolve(repo,"web/assets",u.pathname.slice(1));
    if (!path.startsWith(resolve(repo,"web/assets")+"/")) return reply({error:"Path"},400);
    const body = await readFile(path);
    res.writeHead(200,{"Content-Type":extname(path)===".js"?"text/javascript":"text/css"}); res.end(body);
  } catch (error) { res.writeHead(500); res.end(String(error)); }
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser = await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page = await browser.newPage({viewport:{width:1350,height:1000}}), errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>window.ready);
  const frame = () => page.frameLocator("iframe.device-indicators-frame");
  await frame().locator('[data-metric="speed"] [data-value]').filter({hasText:"100 Мбит/с"}).waitFor();
  assert.equal(await page.locator('[data-standard-summary]').isVisible(),false);
  assert.equal(await page.locator('iframe').getAttribute('sandbox'),"allow-scripts");
  await page.waitForTimeout(200); // initial sandbox height handshake
  const raw = page.locator('[data-decoded-register="bsr"]');
  await raw.locator('summary').scrollIntoViewIfNeeded();
  await page.waitForTimeout(200);
  await raw.locator('summary').click();
  await raw.locator('.decoded-body').waitFor({state:'visible'});
  await page.evaluate(()=>{document.querySelector('iframe').dataset.original="yes";});
  cache.find(e=>e.reg===31).val=0x1004;
  await frame().locator('[data-metric="speed"] [data-value]').filter({hasText:"10 Мбит/с"}).waitFor();
  await page.evaluate(()=>window.page.refresh(true));
  assert.equal(await page.locator('iframe').getAttribute('data-original'),"yes");
  assert.equal(await raw.getAttribute('open'),"");
  assert.equal(calls.filter(c=>c.path==="/api/ui/file").length,2,"assets must not be refetched per snapshot");
  cacheFail=true;
  await page.locator('[data-map-fresh]').filter({hasText:"Связь потеряна"}).waitFor();
  cacheFail=false;
  await page.locator('[data-map-fresh]').filter({hasText:"Данные последнего"}).waitFor();
  cache.find(e=>e.reg===31).valid=false;
  await frame().locator('[data-metric="speed"] [data-value]').filter({hasText:"Нет данных"}).waitFor();
  // A long wait crosses many poll boundaries; rows/open sections stay intact.
  await page.waitForTimeout(1700);
  assert.equal(await raw.getAttribute('open'),"");
  await page.setViewportSize({width:390,height:844});
  await page.waitForTimeout(150);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if (process.env.BVSTK_WEB_SCREENSHOT) await page.screenshot({path:process.env.BVSTK_WEB_SCREENSHOT,fullPage:true});
  await page.evaluate(()=>window.openBus('i2c'));
  await page.getByText("Карта регистров не указана в профиле устройства.").waitFor();
  assert.equal(await page.locator('iframe').count(),0);
  mapMissing=true; await page.evaluate(()=>window.openBus());
  await page.locator('[data-map-message]').filter({hasText:"Не удалось загрузить карту"}).waitFor();
  mapMissing=false;
  viewText='window.bvstkView={apiVersion:1,mount(){},update(){throw Error("Fixture error")}};';
  await page.evaluate(()=>window.openBus());
  await page.locator('[data-map-message]').filter({hasText:"Fixture error"}).waitFor();
  assert.equal(await page.locator('[data-standard-summary]').isVisible(),true);
  // Deliberately hostile fixture confirms no parent DOM or network capability.
  viewText=`window.bvstkView={apiVersion:1,mount(root){try{parent.document.body;}catch{root.textContent='DOM isolated';} fetch('/forbidden',{method:'PUT'}).then(()=>root.textContent='FAILED').catch(()=>root.textContent+=' network blocked');},update(){}};`;
  await page.evaluate(()=>window.openBus());
  await frame().getByText("DOM isolated network blocked").waitFor();
  // The same production page renders the PMIC bundle, using only cached data.
  i2cAssets=true;
  cache=[entry(0,4),entry(0x10,0x3f),entry(0x11,0x62),entry(0x12,0x18),entry(0x13,18),
    entry(0x14,60),entry(0x15,0),entry(0x16,0),entry(0x17,44),entry(0x18,6),entry(0x1a,0),entry(0x1b,0),
    entry(0x1e,0x81),entry(0x1f,7),entry(0x2b,0x1a),entry(0x2c,0),entry(0x2e,15),entry(0x32,0x24),entry(0x48,0),entry(0x49,0)];
  await page.setViewportSize({width:1420,height:1100});
  await page.evaluate(()=>window.openBus('i2c'));
  const axpVoltage=()=>frame().locator('[data-summary="dcdc1_voltage"] [data-value]');
  await axpVoltage().filter({hasText:"3300 мВ"}).waitFor();
  await frame().locator('[data-summary="cpusldo_voltage"] [data-value]').filter({hasText:"VDDR/2"}).waitFor();
  assert.equal(await page.locator('[data-decoded-register]').count(),74);
  assert.equal(await frame().locator('[data-rail]').count(),22);
  assert.equal(await page.locator('[data-map-message]').innerText(),"");
  assert.equal(await page.locator('[data-standard-summary]').isVisible(),false);
  assert.match(await page.locator('[data-map-identity]').innerText(),/проверка идентификатора не задана/);
  const irq=()=>frame().locator('details[data-section="interrupts"]');
  await irq().evaluate(el=>el.open=true);
  await page.evaluate(()=>document.querySelector('iframe').dataset.axpOriginal="yes");
  cache.find(e=>e.reg===0x13).val=17;
  await axpVoltage().filter({hasText:"3200 мВ"}).waitFor();
  cache.find(e=>e.reg===0x48).val=0x02;
  await frame().locator('[data-field="irq_status1.temperature2"] [data-value]').first().filter({hasText:"Отмечено в кэше"}).waitFor();
  assert.equal(await frame().locator('[data-field="irq_status1.temperature2"] [data-value]').first().getAttribute('class'),"bad");
  await page.evaluate(()=>window.page.refresh(true));
  assert.equal(await page.locator('iframe').getAttribute('data-axp-original'),"yes");
  assert.equal(await irq().getAttribute('open'),"");
  await irq().evaluate(el=>el.open=false);
  if (process.env.BVSTK_AXP_SCREENSHOT) await page.screenshot({path:process.env.BVSTK_AXP_SCREENSHOT,fullPage:false});
  await page.setViewportSize({width:390,height:844});
  await page.waitForTimeout(200);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  assert(await frame().locator('body').evaluate(el=>el.scrollWidth<=innerWidth+1));
  cache=cache.map(e=>({...e,source:"settings"}));
  await axpVoltage().filter({hasText:"Нет данных от устройства"}).waitFor();
  assert.equal(await axpVoltage().getAttribute('class'),"unknown");
  assert.equal(calls.some(c=>c.method!=="GET" || c.path==="/forbidden"),false);
  // The intentional broken view is handled inside the sandbox, not the host UI.
  assert.deepEqual(errors,[]);
  console.log("Indicators browser: LAN8720 and AXP15060 custom views, live cache, no writes, stable DOM, mobile, seed rejection, missing map, fallback and sandbox passed");
} finally { await browser.close(); await new Promise(r=>server.close(r)); }
