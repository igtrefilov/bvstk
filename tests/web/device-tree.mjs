// Exercise the actual sidebar wrappers: leaf links and switch summaries must
// share their depth-based layout, including across nested switches.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile} from "node:fs/promises";
import {resolve,extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium}=await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const root=resolve(fileURLToPath(new URL("../../web/assets/",import.meta.url)));
const devices=[
  {name:"axp15060",presence:"responding"},
  {name:"pca9545a",presence:"unknown",topology:{parent:"",channels:4}},
  {name:"pcf8563",presence:"unknown",topology:{parent:"pca9545a",channel:0}},
  {name:"p24c02a",presence:"unknown",topology:{parent:"pca9545a",channel:0}},
  {name:"pca9555",presence:"unknown",topology:{parent:"pca9545a",channel:3}},
];
const server=http.createServer(async(req,res)=>{
  try {
    if(req.url==="/") {
      res.writeHead(200,{"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head>
      <body><aside id="sidebar" class="sidebar open"><nav>
      <details class="nav-branch" open><summary class="nav-group">Подсистемы</summary><div class="nav-children">
      <a href="#i2c">I²C</a><div class="device-tree" data-device-tree="i2c"></div>
      <a href="#mdio">MDIO</a><div class="device-tree" data-device-tree="mdio"></div>
      </div></details></nav></aside><script type="module">
      import {updateDeviceTree} from "/js/device-tree.js";
      window.devices=${JSON.stringify(devices)};
      window.updateTree=()=>updateDeviceTree(document.querySelector('[data-device-tree="i2c"]'),"i2c",window.devices);
      window.updateTree();
      updateDeviceTree(document.querySelector('[data-device-tree="mdio"]'),"mdio",[{name:"lan8720",presence:"responding"}]);
      window.ready=true;
      </script></body></html>`);
    }
    if(req.url==="/favicon.ico"){res.writeHead(204);return res.end();}
    const file=resolve(root,req.url.slice(1));
    assert(file.startsWith(root+"/"));
    res.writeHead(200,{"Content-Type":extname(file)===".js"?"text/javascript":"text/css"});
    res.end(await readFile(file));
  } catch(error) {if(!res.headersSent)res.writeHead(500);res.end(String(error));}
});
await new Promise(r=>server.listen(0,"127.0.0.1",r));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page=await browser.newPage({viewport:{width:1300,height:950}}),errors=[];
  page.on("pageerror",e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>window.ready);
  const link=name=>page.locator(`[data-device-name="${name}"]`);
  const branch=key=>page.locator(`[data-branch="${key}"]`);
  const open=key=>branch(key).evaluate(el=>el.open);
  async function toggle(key) {
    const summary=branch(key).locator(":scope > summary"),wasOpen=await open(key);
    const box=await summary.boundingBox();assert(box);
    // The chevron area must toggle the branch without following the device link.
    await summary.click({position:{x:box.width-12,y:box.height/2}});
    assert.equal(await open(key),!wasOpen,key);
  }
  const arrow=key=>branch(key).locator(":scope > summary").evaluate(el=>getComputedStyle(el,"::after").transform);
  async function geometry() {
    return page.evaluate(()=>Object.fromEntries(
      [...document.querySelectorAll("[data-device-name], .topology-channel>summary")].map(el=>{
        const id=el.dataset.deviceName || el.parentElement.dataset.branch;
        return [id,{icon:el.querySelector("svg").getBoundingClientRect().x,
          label:el.querySelector(".device-label").getBoundingClientRect().x}];
      })
    ));
  }
  function aligned(a,b) {assert(Math.abs(a-b)<0.5,`${a} != ${b}`);}
  let positions=await geometry();
  aligned(positions.axp15060.icon,positions.pca9545a.icon);
  aligned(positions.axp15060.label,positions.pca9545a.label);
  aligned(positions.axp15060.icon,positions.lan8720.icon);
  assert.equal(await link("pcf8563").isVisible(),false);
  assert.equal(await branch("pca9545a:0").isVisible(),false);
  assert.equal(await arrow("device:pca9545a"),"none");
  await toggle("device:pca9545a");
  assert.notEqual(await arrow("device:pca9545a"),"none");
  assert.equal(await branch("pca9545a:0").isVisible(),true);
  assert.equal(await link("pcf8563").isVisible(),false);
  await toggle("pca9545a:0");
  await toggle("pca9545a:3");
  assert.notEqual(await arrow("pca9545a:0"),"none");
  for(const width of [1300,1024,390]) {
    await page.setViewportSize({width,height:950});
    positions=await geometry();
    aligned(positions.axp15060.icon,positions.pca9545a.icon);
    aligned(positions.axp15060.label,positions.pca9545a.label);
    aligned(positions["pca9545a:0"].icon,positions.pca9545a.icon+16);
    aligned(positions["pca9545a:0"].label,positions.pca9545a.label+16);
    for(const name of ["pcf8563","p24c02a","pca9555"]) {
      assert.equal(await link(name).isVisible(),true);
      aligned(positions[name].icon,positions.pca9545a.icon+32);
      aligned(positions[name].label,positions.pca9545a.label+32);
    }
    assert(await page.locator(".sidebar nav").evaluate(el=>el.scrollWidth<=el.clientWidth+1));
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  }
  await link("pca9545a").click();
  assert.equal(await page.evaluate(()=>location.hash),"#i2c?name=pca9545a");
  assert.equal(await open("device:pca9545a"),true);
  await link("pcf8563").focus();
  await page.evaluate(()=>{
    document.querySelector('[data-device-name="pcf8563"]').dataset.retained="yes";
    window.devices[2].presence="responding";window.updateTree();
  });
  assert.equal(await link("pcf8563").getAttribute("data-retained"),"yes");
  assert.equal(await page.evaluate(()=>document.activeElement.dataset.deviceName),"pcf8563");
  assert.equal(await open("pca9545a:0"),true);
  await toggle("device:pca9545a");
  assert.equal(await link("pcf8563").isVisible(),false);
  await page.evaluate(()=>window.updateTree());
  assert.equal(await open("device:pca9545a"),false);
  await toggle("device:pca9545a");
  assert.equal(await link("pcf8563").isVisible(),true);
  // Reuse the same renderer for a second switch level, with no model-specific CSS.
  await page.evaluate(()=>{
    window.devices.push(
      {name:"inner_switch",presence:"unknown",topology:{parent:"pca9545a",channel:0,channels:2}},
      {name:"nested_device",presence:"unknown",topology:{parent:"inner_switch",channel:1}}
    );window.updateTree();
  });
  assert.equal(await open("pca9545a:0"),true);
  await toggle("device:inner_switch");
  await toggle("inner_switch:1");
  positions=await geometry();
  aligned(positions.inner_switch.icon,positions.pcf8563.icon);
  aligned(positions.nested_device.icon,positions.inner_switch.icon+32);
  assert.equal(await link("nested_device").isVisible(),true);
  assert.deepEqual(errors,[]);
  console.log("Device tree: equal root indentation, nested channels/devices, chevrons, collapse, focus, mobile layout and nested switches passed");
} finally {
  await browser.close();await new Promise(r=>server.close(r));
}
