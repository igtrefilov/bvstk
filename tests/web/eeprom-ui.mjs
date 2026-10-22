// Real EEPROM bundle in the production HTTP/cache UI and opaque-origin iframe.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile, mkdir} from "node:fs/promises";
import {resolve, extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo = resolve(fileURLToPath(new URL("../../", import.meta.url)));
const bundle = "configs/i2c/pca9545a/p24c02a/";
let cache = [], clockMs = 2000, presence = "responding", fail = false, polls = 0, writes = 0;
const put = (reg, val, updated_ms = clockMs, extra = {}) => {
  const entry = {reg,val,updated_ms,valid:true,source:"I2C",...extra};
  const index = cache.findIndex(e => e.reg === reg);
  if (index < 0) cache.push(entry); else cache[index] = entry;
};
const server = http.createServer(async (req, res) => {
  try {
    if (req.method !== "GET") {writes++; throw Error("Unexpected mutation");}
    const u = new URL(req.url, "http://localhost"), q = Object.fromEntries(u.searchParams);
    const json = data => {res.writeHead(200, {"Content-Type":"application/json"}); res.end(JSON.stringify(data));};
    if (u.pathname === "/api/ui/bus") {
      if (!q.name) return json({ready:true,devices:[{name:"p24c02a",presence}]});
      assert.equal(q.name, "p24c02a");
      if (q.action) {
        assert.equal(q.action, "cache"); polls++;
        if (fail) {res.writeHead(503); return res.end("temporary disconnect");}
        return json({cache,uptime_ms:clockMs,presence});
      }
      return json({config:{name:"p24c02a",addr_7b:87,reg_count:256,max_value_code:255,
        policy:"whitelist",whitelist:[],blacklist:[],settings:[]},cache,presence,uptime_ms:clockMs,
        file_name:"pca9545a/p24c02a/p24c02a.json",register_map:"register_map.json",view:"view.js",
        saved:true,revision:"1"});
    }
    if (u.pathname === "/api/ui/file") {
      assert.equal(q.volume, "flash"); const asset = q.path.split("/").at(-1);
      assert(["register_map.json", "view.js"].includes(asset));
      assert.equal(q.path, "/config/i2c/pca9545a/p24c02a/" + asset);
      res.writeHead(200, {"Content-Type":"application/octet-stream"});
      return res.end(await readFile(resolve(repo, bundle, asset)));
    }
    if (u.pathname === "/") {
      res.writeHead(200, {"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html lang="ru"><head><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/css/app.css"></head><body><main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
        import {busPage} from "/js/buses.js";
        window.page = await busPage(document.getElementById('root'), new URLSearchParams('name=p24c02a&tab=indicators'),
          {signal:new AbortController().signal,setContext(){}}, 'i2c'); window.ready = true;
      </script></body></html>`);
    }
    if (u.pathname === "/favicon.ico") {res.writeHead(204); return res.end();}
    const file = resolve(repo, "web/assets", u.pathname.slice(1));
    assert(file.startsWith(resolve(repo, "web/assets") + "/"));
    res.writeHead(200, {"Content-Type":extname(file) === ".js" ? "text/javascript" : "text/css"});
    res.end(await readFile(file));
  } catch (error) {if (!res.headersSent) res.writeHead(500); res.end(String(error));}
});
await new Promise(r => server.listen(0, "127.0.0.1", r));
const browser = await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome",args:["--no-sandbox"]});
try {
  const page = await browser.newPage({viewport:{width:1440,height:1100},acceptDownloads:true}), errors = [];
  page.on("pageerror", e => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.ready);
  const frame = page.frameLocator("iframe.device-indicators-frame");
  await frame.locator("#known-count").filter({hasText:/^0$/}).waitFor();
  const documentFrame = page.frames().find(f => f !== page.mainFrame());
  assert.equal(await frame.locator(".byte").count(),256);
  assert(await frame.locator("#download").isDisabled());
  assert.equal(await page.locator("iframe").getAttribute("sandbox"), "allow-scripts allow-downloads");
  assert(await documentFrame.evaluate(() => {try {void parent.document;return false;} catch {return true;}}));
  put(0, 99, 1000, {source:"settings"}); put(1, 88, 1000, {valid:false}); put(255, 0xA5, 1000);
  await frame.locator("#known-count").filter({hasText:/^1$/}).waitFor();
  assert.equal(await frame.locator('[data-byte="0"]').innerText(), "—");
  await frame.locator('[data-byte="255"]').click();
  assert.equal(await frame.locator("#selected-value").innerText(), "0xA5");
  assert.equal(await frame.locator("#selected-page").innerText(), "0xF8–0xFF");
  clockMs = 3000; put(255, 0xA6);
  await frame.locator("#selected-value").filter({hasText:"0xA6"}).waitFor();
  assert.equal(await frame.locator("#changed-count").innerText(), "1");
  assert.equal(await frame.locator("#history-body tr").count(), 1);
  await documentFrame.evaluate(() => {window.originalCell=document.querySelector('[data-byte="255"]');});
  clockMs = 4000; put(255, 0xA6);
  await frame.locator("#selected-time").filter({hasText:"00:00:04.000"}).waitFor();
  assert.equal(await frame.locator("#history-body tr").count(), 1);
  assert(await documentFrame.evaluate(() => document.activeElement === window.originalCell && window.originalCell === document.querySelector('[data-byte="255"]')));
  // A typed address must survive an asynchronous snapshot before Enter.
  await frame.locator("#address-input").fill("80");
  clockMs = 5000; put(255, 0xA7);
  await frame.locator("#selected-value").filter({hasText:"0xA7"}).waitFor();
  assert.equal(await frame.locator("#address-input").inputValue(), "80");
  await frame.locator("#address-input").press("Enter");
  assert.equal(await frame.locator("#selected-address").innerText(), "0x80");
  await frame.locator('[data-byte="128"]').press("ArrowLeft");
  assert.equal(await frame.locator("#selected-address").innerText(), "0x7F");

  clockMs = 6000;
  for (let i = 0; i < 256; i++) put(i, i);
  const untrusted = "<img src=x onerror=alert(1)>";
  for (let i = 0; i < untrusted.length; i++) put(i, untrusted.charCodeAt(i));
  await frame.locator("#known-count").filter({hasText:/^256$/}).waitFor();
  assert.equal(await frame.locator("img").count(), 0);
  assert.equal(await frame.locator("#download").isDisabled(),false);
  await frame.locator("#pause").click();
  const frozen = await frame.locator("#device-root").innerText(), beforePolls = polls;
  clockMs = 7000; put(127, 254); put(20, 1);
  const deadline = Date.now() + 8000;
  while (polls < beforePolls + 2 && Date.now() < deadline) await new Promise(r => setTimeout(r, 100));
  assert(polls >= beforePolls + 2, "polling continues during pause");
  assert.equal(await frame.locator("#device-root").innerText(), frozen, "pause freezes all visible values, timestamps and history");
  const pendingDownload = page.waitForEvent("download"); await frame.locator("#download").click();
  const download = await pendingDownload;
  const bytes = await readFile(await download.path());
  assert.equal(bytes.length,256); assert.equal(bytes[127],127, "download uses the frozen snapshot");
  assert.match(download.suggestedFilename(), /^p24c02a-.*\.bin$/);
  await frame.locator("#pause").click();
  await frame.locator("#selected-value").filter({hasText:"0xFE"}).waitFor();

  fail = true;
  await frame.locator("#state-banner").filter({hasText:"Связь с bvstk потеряна"}).waitFor();
  assert(await frame.locator("#download").isDisabled());
  assert.equal(await frame.locator("#selected-value").innerText(), "0xFE");
  assert.match(await frame.locator('[data-byte="127"]').getAttribute("class"), /old/);
  fail = false; presence = "unavailable"; cache.forEach(e => e.valid = false);
  await frame.locator("#state-banner").filter({hasText:"Устройство не отвечает"}).waitFor();
  assert.equal(await frame.locator("#known-count").innerText(), "0");
  const beforeReconnect = await frame.locator("#history-meta").innerText();
  clockMs = 8000; presence = "responding"; put(127, 100);
  await frame.locator("#selected-value").filter({hasText:"0x64"}).waitFor();
  assert.equal(await frame.locator("#history-meta").innerText(), beforeReconnect);
  // Reboot while paused: old export stays frozen until resume; new session clears history.
  await frame.locator("#pause").click();
  clockMs = 50; cache = []; put(127, 3, 20);
  const oldPolls = polls;
  const rebootDeadline = Date.now() + 8000;
  while (polls < oldPolls + 2 && Date.now() < rebootDeadline) await new Promise(r => setTimeout(r, 100));
  assert(polls >= oldPolls + 2);
  assert.equal(await frame.locator("#selected-value").innerText(), "0x64");
  await frame.locator("#pause").click();
  await frame.locator("#selected-value").filter({hasText:"0x03"}).waitFor();
  assert.equal(await frame.locator("#changed-count").innerText(), "0");
  assert.equal(await frame.locator('[data-byte="255"]').innerText(), "—");
  assert.equal(await frame.locator("#history-body .empty-log").count(), 1);

  clockMs = 1000; for (let i = 0; i < 256; i++) put(i, i);
  await frame.locator("#known-count").filter({hasText:/^256$/}).waitFor();
  await page.setViewportSize({width:390,height:850});
  await page.waitForTimeout(200);
  assert(await documentFrame.evaluate(() => document.documentElement.scrollWidth <= innerWidth), "mobile iframe must not overflow");
  await documentFrame.evaluate(() => {document.querySelector('#memory-scroll').scrollLeft=180; document.querySelector('#history-panel').open=false;});
  await frame.locator('[data-byte="250"]').click();
  const scroll = await documentFrame.evaluate(() => document.querySelector('#memory-scroll').scrollLeft);
  clockMs = 2000; put(250, 2);
  await frame.locator("#selected-value").filter({hasText:"0x02"}).waitFor();
  assert.equal(await documentFrame.evaluate(() => document.querySelector('#memory-scroll').scrollLeft), scroll);
  assert.equal(await frame.locator("#history-panel").getAttribute("open"), null);
  if (process.env.BVSTK_EEPROM_SCREENSHOTS) {
    const directory = process.env.BVSTK_EEPROM_SCREENSHOTS; await mkdir(directory, {recursive:true});
    // Give Chrome an actual viewport covering the tall cross-origin iframe;
    // captureBeyondViewport alone can leave its off-screen tiles unpainted.
    for (const [name,width] of [["mobile",390],["desktop",1440]]) {
      await page.setViewportSize({width,height:1100}); await page.waitForTimeout(200);
      const height = await page.evaluate(() => document.documentElement.scrollHeight);
      await page.setViewportSize({width,height:height + 50});
      await documentFrame.evaluate(() => window.scrollTo(0,0));
      await page.evaluate(() => window.scrollTo(0,0)); await page.waitForTimeout(250);
      await page.screenshot({path:resolve(directory,name + ".png"),fullPage:true});
    }
  }
  assert.equal(writes,0); assert.deepEqual(errors,[]);
  console.log("EEPROM UI: physical cache, stable selection/scroll/focus, pause, real sandbox download, gaps/reboot, mobile and no bus writes passed");
} finally {await browser.close(); await new Promise(r => server.close(r));}
