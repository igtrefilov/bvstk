// Real SFP bundle in the actual indicators sandbox. Fixture data stays local.
import assert from "node:assert/strict";
import http from "node:http";
import {readFile} from "node:fs/promises";
import {resolve, extname} from "node:path";
import {fileURLToPath} from "node:url";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const repo = resolve(fileURLToPath(new URL("../../", import.meta.url)));
const bundle = "configs/i2c/pca9545a/sfp/";
const spaces = [{id:"identity", address:80, first:0, register_count:128},
  {id:"diagnostics", address:81, first:128, register_count:128}];
const cache = [];
const put = (reg, val) => {
  const next = {reg, val, valid:true, source:"I2C", updated_ms:1000};
  const i = cache.findIndex(e => e.reg === reg);
  if (i < 0) cache.push(next); else cache[i] = next;
};
let writes = 0, clockMs = 1500, presence = null, cacheFail = false;
const server = http.createServer(async (req, res) => {
  try {
    const u = new URL(req.url, "http://localhost"), q = Object.fromEntries(u.searchParams);
    const json = value => {res.writeHead(200, {"Content-Type":"application/json"}); res.end(JSON.stringify(value));};
    if (req.method !== "GET") {writes++; throw new Error("Unexpected bus/file write");}
    if (u.pathname === "/api/ui/bus") {
      if (!q.name) return json({ready:true, devices:[{name:"sfp", presence:"unknown", address_spaces:spaces}]});
      if (q.action === "cache") {
        if (cacheFail) {res.writeHead(503);return res.end("temporarily unavailable");}
        return json({cache, uptime_ms:clockMs, presence:presence || (cache.length ? "responding" : "unknown")});
      }
      assert.equal(q.name, "sfp"); assert(!q.action);
      return json({config:{name:"sfp", reg_count:256, addr_7b:80, max_value_code:255,
        policy:"whitelist", whitelist:[], blacklist:[], settings:[]},
      cache, address_spaces:spaces, file_name:"pca9545a/sfp/sfp.json",
      register_map:"register_map.json", view:"view.js", presence:"unknown", saved:true, revision:"1"});
    }
    if (u.pathname === "/api/ui/file") {
      assert.equal(q.volume, "flash");
      const name = q.path.split("/").at(-1);
      assert(["register_map.json", "view.js"].includes(name));
      assert.equal(q.path, "/config/i2c/pca9545a/sfp/" + name);
      res.writeHead(200, {"Content-Type":name.endsWith(".json") ? "application/json" : "text/javascript"});
      return res.end(await readFile(resolve(repo, bundle, name)));
    }
    if (u.pathname === "/") {
      res.writeHead(200, {"Content-Type":"text/html; charset=utf-8"});
      return res.end(`<!doctype html><html><head><link rel="stylesheet" href="/css/app.css"></head><body>
        <main id="root" style="margin:0"></main><div id="notice" class="hidden"></div><script type="module">
        import {busPage} from "/js/buses.js";
        window.controller=await busPage(document.getElementById("root"),
          new URLSearchParams({name:"sfp",tab:"indicators"}),
          {signal:new AbortController().signal,setContext(){}},"i2c");window.ready=true;
        </script></body></html>`);
    }
    if (u.pathname === "/favicon.ico") {res.writeHead(204); return res.end();}
    const path = resolve(repo, "web/assets", u.pathname.slice(1));
    assert(path.startsWith(resolve(repo, "web/assets") + "/"));
    res.writeHead(200, {"Content-Type":extname(path) === ".js" ? "text/javascript" : "text/css"});
    res.end(await readFile(path));
  } catch (e) {if (!res.headersSent) res.writeHead(500); res.end(String(e));}
});
await new Promise(r => server.listen(0, "127.0.0.1", r));
const browser = await chromium.launch({headless:true,
  executablePath:process.env.BVSTK_CHROME || "/usr/bin/google-chrome", args:["--no-sandbox","--window-size=1600,1200"]});
try {
  const page = await browser.newPage({viewport:{width:1400, height:1000}}), errors = [];
  page.on("pageerror", e => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.ready);
  const frame = page.frameLocator("iframe");
  await frame.locator(".notice").filter({hasText:"ещё не прочитан"}).waitFor();
  assert.equal(await frame.locator('[data-metric="temperature"] .value').innerText(), "Нет данных");
  put(0, 3); put(92, 0x68); put(93, 0xF0); put(238, 0); put(224, 25); put(225, 0);
  await frame.locator('[data-metric="temperature"] .value').filter({hasText:"25.00 °C"}).waitFor();
  put(224, 26);
  await frame.locator('[data-metric="temperature"] .value').filter({hasText:"26.00 °C"}).waitFor();
  // EEPROM content is untrusted text, never injected HTML.
  for (let i = 0; i < 16; i++) put(20 + i, "<b>TEST</b>".padEnd(16).charCodeAt(i));
  await frame.locator('[data-metric="vendor"] .value').filter({hasText:"<b>TEST</b>"}).waitFor();
  assert.equal(await frame.locator('[data-metric="vendor"] b').count(), 0);
  await page.waitForTimeout(200); // let the sandbox publish its final layout height
  await page.locator("article.card > details > summary").focus();
  await page.locator("article.card > details > summary").press("Enter");
  assert.equal(await page.locator('[data-decoded-register="a0_00"] [data-raw]').innerText(), "0x03");
  assert.equal(await page.locator('[data-decoded-register="a2_60"] [data-raw]').innerText(), "0x1A");
  await frame.locator('[data-metric="temperature"]').click();
  await frame.locator("#metric-inspector").waitFor();
  assert.match(await frame.locator("#metric-inspector").innerText(), /diagnostics:0x60–0x61/);
  await page.waitForTimeout(1800);
  assert.equal(await frame.locator('[data-metric="temperature"]').getAttribute("aria-expanded"), "true");
  await frame.locator("#metric-inspector .close").click();
  // A new sample may have exactly the same value: the timestamp still advances.
  for (const reg of [224,225,238]) cache.find(e => e.reg === reg).updated_ms = 2000;
  clockMs = 2500;
  await frame.locator('[data-metric="temperature"] .time').filter({hasText:"00:00:02.000"}).waitFor();
  const history = frame.locator("summary").filter({hasText:"История измерений"});
  await history.focus(); await history.press("Enter");
  const sfpFrame = page.frames().find(f => f !== page.mainFrame());
  await sfpFrame.waitForFunction(() => document.querySelector('path')?.getAttribute('d')?.includes('L'));
  clockMs = 8000;
  await frame.locator('[data-metric="temperature"] .level').filter({hasText:"устарели"}).waitFor();
  assert.match(await frame.locator('[data-metric="temperature"] .value').getAttribute("class"), /unknown/);
  cacheFail = true;
  await frame.locator(".notice").filter({hasText:"Связь с bvstk потеряна"}).waitFor();
  cacheFail = false; presence = "unavailable"; cache.forEach(e => e.valid = false);
  await frame.locator('[data-metric="presence"] .value').filter({hasText:"Не готов"}).waitFor();
  await frame.locator('[data-metric="temperature"] .value').filter({hasText:"Нет данных"}).waitFor();
  await page.setViewportSize({width:420,height:900});
  const iframe = page.frames().find(f => f !== page.mainFrame());
  assert(await iframe.evaluate(() => document.body.scrollWidth <= document.documentElement.clientWidth), "mobile form must not overflow horizontally");
  assert.deepEqual(errors, []); assert.equal(writes, 0);
  console.log("SFP UI: live DDM, stable controls, history, stale/offline/missing data, mobile, safe text and no bus writes passed");
} finally {
  await browser.close(); await new Promise(r => server.close(r));
}
