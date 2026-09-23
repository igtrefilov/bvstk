// Browser regression fixture. API responses are simulated; this is not a
// substitute for PS/PL hardware validation. No writes to the board.
import http from "node:http";
import {readFile, mkdir} from "node:fs/promises";
import {resolve, extname} from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";
import assert from "node:assert/strict";
const {chromium} = await import(process.env.BVSTK_PLAYWRIGHT_MODULE || "playwright");
const root = resolve(fileURLToPath(new URL("../../web/assets/",import.meta.url)));
const output = process.env.BVSTK_WEB_SCREENSHOTS;
if (output) await mkdir(output,{recursive:true});
const configurations = {
  mdio:{name:"lan8720",phy_addr:1,reg_count:32,policy:"whitelist",whitelist:[0,4,18,30].map(reg=>({reg,val:0,mask:0})),blacklist:[],settings:[],autopoll_enabled:true,autopoll_regs:[0,1,2,3,4,5,17,31],autopoll_reg_delay_ms:0,autopoll_cycle_delay_ms:50},
  i2c:{name:"axp15060",addr_7b:54,reg_count:74,max_value_code:64,policy:"whitelist",whitelist:[{reg:19,val:16}],blacklist:[],settings:[]}
};
const revision={mdio:1,i2c:1},saved={mdio:true,i2c:true};
const calls=[]; let conflict=false, failRtos=false, mdioListState="devices", versionMode="normal";
const files = new Map([["sd-pl:/test1",null],["sd-pl:/roundtrip.json",Buffer.from('{"example":true}\n')],["sd-pl:/Firmware.tif",Buffer.alloc(223423)]]);
const server=http.createServer(async(req,res)=>{
  const u=new URL(req.url,"http://local"),q=Object.fromEntries(u.searchParams),chunks=[];
  for await(const chunk of req)chunks.push(chunk);
  const body=Buffer.concat(chunks);
  const reply=(value,status=200)=>{res.writeHead(status,{"Content-Type":"application/json"});res.end(JSON.stringify(value));};
  const error=(message,status)=>reply({ok:false,error:message},status);
  calls.push([req.method,u.pathname,q]);
  if(u.pathname==="/api/net")return reply({ip:"192.168.0.10",netmask:"255.255.255.0",gateway:"192.168.0.1",mac:"00:0a:35:00:01:02",mode:"static",up:true,link_up:true});
  if(u.pathname==="/api/rtos")return failRtos ? error("RTOS unavailable",503) : reply({uptime_ms:759000,heap_free:396697,heap_min_ever:367718,tick_rate_hz:100});
  if(u.pathname==="/api/version"){
    if(versionMode==="silent")return;
    if(versionMode==="error")return error("Version temporarily unavailable",503);
    if(versionMode==="malformed"){res.setHeader('Content-Type','application/json');res.end('invalid JSON');return;}
    return reply({build_date:"Sep 22 2026",build_time:"16:00:00",http_port:80});
  }
  if(u.pathname==="/api/ui/volumes")return reply({volumes:[{name:"flash",ready:true,total_bytes:25165824,free_bytes:22858956},{name:"sd",ready:true,total_bytes:134217728000,free_bytes:134217728000},{name:"sd-pl",ready:true,total_bytes:378535936,free_bytes:null,fat_bits:32}]});
  if(u.pathname==="/api/ui/sd-pl")return reply({ready:true,mounted:true,filesystem_owned:true,ocr:3237978240,block_addressed:true,last_lba:1,last_error:"ok",fat_bits:32,volume_lba:0,volume_sectors:739328});
  if(u.pathname==="/api/ui/bus"){
    const b=q.bus,c=configurations[b];
    if(req.method==="GET"){
      if(!q.name&&b==="mdio"&&mdioListState!=="devices")return reply({ready:mdioListState==="empty",devices:[]});
      if(!q.name)return reply({ready:true,devices:[{name:c.name,address:b==="mdio"?c.phy_addr:c.addr_7b,policy:c.policy,available:true}]});
      return reply({config:c,revision:String(revision[b]),saved:saved[b],available:true,file_name:c.name+".json",cache:Array.from({length:32},(_,reg)=>({reg,val:reg===2?7:reg===3?49393:0,valid:reg<8,source:"PHY",age_ms:100})),stats:{reads:128,writes:3,denied:0,errors:0,observed:3}});
    }
    if(q.action==="apply"||q.action==="save"){
      if(conflict||q.revision!==String(revision[b]))return error("configuration changed",409);
      if(q.action==="apply"){configurations[b]=JSON.parse(body);revision[b]++;saved[b]=false;}
      else saved[b]=true;
    }
    return reply(q.action==="read"?{ok:true,reg:Number(q.reg),val:7}:{ok:true});
  }
  if(u.pathname==="/api/ui/files"){
    const key=q.volume+":"+q.path;
    if(req.method==="GET"){
      if(q.action==="stat")return files.has(key)?reply({directory:files.get(key)===null,size:files.get(key)?.length||0}):error("not found",404);
      const prefix=key.endsWith("/")?key:key+"/";
      return reply({ok:true,entries:[...files].filter(([p])=>p.startsWith(prefix)&&!p.slice(prefix.length).includes("/")).map(([p,v])=>({name:p.slice(prefix.length),directory:v===null,size:v?.length||0,date:0,time:0})),next:null});
    }
    if(q.action==="mkdir")files.set(key,null);
    if(q.action==="remove")files.delete(key);
    if(q.action==="rename"||q.action==="copy"){
      const dst=q.dst_volume+":"+q.dst;
      if(files.has(dst))return error("exists",409);
      files.set(dst,files.get(key));if(q.action==="rename")files.delete(key);
    }
    return reply({ok:true});
  }
  if(u.pathname==="/api/ui/file"){
    const key=q.volume+":"+q.path;
    if(req.method==="PUT"){if(files.has(key)&&q.overwrite!=="true")return error("exists",409);files.set(key,body);return reply({ok:true,bytes:body.length});}
    if(!files.has(key))return error("not found",404);
    res.writeHead(200,{"Content-Type":"application/octet-stream"});res.end(files.get(key));return;
  }
  const file=resolve(root,"."+(u.pathname==="/"?"/index.html":u.pathname));
  if(!file.startsWith(root+"/"))return error("denied",403);
  try{const data=await readFile(file);res.setHeader("Content-Type",({".js":"application/javascript",".css":"text/css",".html":"text/html",".png":"image/png"})[extname(file)]||"application/octet-stream");res.end(data);}catch{return error("not found",404);}
});
await new Promise(resolve=>server.listen(0,"127.0.0.1",resolve));
const browser=await chromium.launch({headless:true,executablePath:process.env.BVSTK_CHROME||"/usr/bin/google-chrome",args:["--no-sandbox","--no-proxy-server"]});
const page=await browser.newPage({viewport:{width:1586,height:992},deviceScaleFactor:1});
const errors=[];page.on("pageerror",e=>errors.push(e.message));
const screenshot=async name=>{if(output)await page.screenshot({path:resolve(output,name+".png"),fullPage:true,animations:"disabled"});};
async function checkSidebarArtwork() {
  const layout = await page.evaluate(()=>{
    const sidebar=document.getElementById('sidebar'), waves=document.querySelector('.brand-waves');
    const panel=sidebar.getBoundingClientRect(), box=waves.getBoundingClientRect();
    const bird=document.querySelector('.brand-bird').getBoundingClientRect();
    const art=document.querySelector('.sidebar-art').getBoundingClientRect();
    const nav=sidebar.querySelector('nav').getBoundingClientRect();
    const drawing=waves.getBBox(), view=waves.viewBox.baseVal;
    return {left:box.left-panel.left-sidebar.clientLeft,width:box.width-sidebar.clientWidth,bottom:box.bottom-panel.bottom,
      birdWidth:bird.width,birdHeight:bird.height,underBird:box.top-bird.bottom,navGap:art.top-nav.bottom,
      reachesLeft:drawing.x<=view.x,reachesRight:drawing.x+drawing.width>=view.x+view.width,
      reachesBottom:drawing.y+drawing.height>=view.y+view.height};
  });
  for(const edge of ['left','width','bottom'])assert(Math.abs(layout[edge])<=1,edge+': '+JSON.stringify(layout));
  assert(Math.abs(layout.birdWidth-90)<.1);assert(Math.abs(layout.birdHeight-42)<.1);
  assert(layout.underBird>=-1&&layout.navGap>=-1);
  assert(layout.reachesLeft&&layout.reachesRight&&layout.reachesBottom);
}
async function checkConnection(online, timeout=6000) {
  const indicator=page.locator('#connection[data-state="'+(online?'online':'offline')+'"]');
  await indicator.waitFor({timeout});
  assert.equal(await indicator.textContent(),online?'Онлайн':'Оффлайн');
  assert.equal(await indicator.locator('.host').count(),0);
  assert.equal(await indicator.locator('.dot').evaluate(el=>getComputedStyle(el).backgroundColor),online?'rgb(55, 173, 97)':'rgb(160, 171, 181)');
}
const versionCalls=()=>calls.filter(([method,path])=>method==='GET'&&path==='/api/version').length;
const idle=()=>page.waitForFunction(async()=>!(await import('/js/api.js')).pending);
try{
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.getByRole("heading",{name:"Состояние подсистем",exact:true}).waitFor();
  assert.equal(await page.locator('#content .page-head, #content .metric-strip').count(),0);
  assert.equal(await page.getByRole('heading',{name:'Сводка состояния',exact:true}).count(),0);
  assert.equal(await page.locator('#content > div > :first-child').getAttribute('class'),'workbench overview-workbench');
  assert.equal(await page.locator('.overview-workbench .card-head h1').textContent(),'Состояние подсистем');
  assert.equal(await page.locator('.overview-workbench .card-head [data-refresh]').count(),1);
  assert.equal(await page.locator(".nav-group").textContent(),"Подсистемы");
  await page.locator('[data-device-name="lan8720"]').waitFor();
  await checkConnection(true);
  const glyph = selector => page.locator(selector+' svg').innerHTML();
  assert.equal(await glyph('[data-nav="i2c"]'),await glyph('[data-nav="mdio"]'));
  assert.equal(await glyph('[data-device-name="axp15060"]'),await glyph('[data-device-name="lan8720"]'));
  assert.notEqual(await glyph('[data-nav="i2c"]'),await glyph('[data-device-name="lan8720"]'));
  assert.equal(await page.locator('#sidebar a.active').count(),1);
  assert.equal(await page.locator('#sidebar').evaluate(el=>getComputedStyle(el).backgroundColor),'rgb(48, 57, 65)');
  await checkSidebarArtwork();
  assert.equal(await page.getByText('Тракт управления',{exact:false}).count(),0);
  await page.locator('[data-subsystem="i2c:axp15060"]').click();
  await page.locator('#overview-inspector').getByText('axp15060',{exact:true}).waitFor();
  await page.locator('[data-subsystem="mdio:lan8720"]').click();
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
  await screenshot("01-overview");
  await page.setViewportSize({width:1280,height:600});
  assert(await page.locator('#sidebar nav').evaluate(el=>el.scrollHeight>el.clientHeight));
  await page.locator('[data-system-nav="maintenance"]').scrollIntoViewIfNeeded();
  await checkSidebarArtwork();
  await page.locator('#sidebar nav').evaluate(el=>{el.scrollTop=0;});
  await page.setViewportSize({width:1586,height:992});
  failRtos=true;await page.locator('[data-refresh]').click();
  await page.locator('.page-foot.error-text').waitFor();
  assert.equal(await page.locator('#content .is-stale').count(),1);
  failRtos=false;await page.locator('[data-refresh]').click();
  await page.waitForFunction(()=>!document.querySelector('#content .is-stale'));
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
  const detailReads = () => calls.filter(([method,path,q])=>method==='GET'&&path==='/api/ui/bus'&&q.bus==='mdio'&&q.name).length;
  const beforeUnavailable=detailReads();
  mdioListState="unready";await page.locator('[data-refresh]').click();
  await page.locator('[data-subsystem="mdio:"]').getByText('Сервис не готов',{exact:true}).waitFor();
  assert.equal(detailReads(),beforeUnavailable);
  mdioListState="empty";await page.locator('[data-refresh]').click();
  await page.locator('[data-subsystem="mdio:"]').getByText('Устройства не настроены',{exact:true}).waitFor();
  assert.equal(detailReads(),beforeUnavailable);
  mdioListState="devices";await page.locator('[data-refresh]').click();
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
  await page.locator('[data-nav="mdio"]').click();
  await page.locator('.device-name').filter({hasText:"lan8720"}).waitFor();
  await page.locator('[data-tab="policy"]').click();
  await screenshot("02-mdio-policy");
  await page.getByRole("button",{name:"Добавить правило"}).click();
  await page.locator('#modal [name="reg"]').fill("0x1F");
  await page.locator('#modal [name="val"]').fill("0x0000");
  await page.locator('#modal [name="mask"]').fill("0x0000");
  await page.getByRole("button",{name:"Готово",exact:true}).click();
  assert(await page.locator("[data-save]").isDisabled());
  await page.locator("[data-apply]").click();
  await page.getByText("Применено в RAM, не сохранено во Flash",{exact:true}).waitFor();
  assert.equal(configurations.mdio.whitelist.length,5);assert.equal(saved.mdio,false);
  await page.locator("[data-save]").click();
  await page.getByText("Конфигурация сохранена",{exact:true}).waitFor();assert(saved.mdio);
  await page.locator('[data-tab="overview"]').click();await page.locator('[data-tab="policy"]').click();
  // Inline comparison uses stable original rows, including deletion before other rows.
  await page.locator('[data-rule="4"]').click();
  await page.locator('[data-rule-field="val"]').fill('0x0002');
  await page.locator('[data-rule-field="mask"]').fill('0x0002');
  assert.equal(await page.locator('[data-rule].changed').count(),1);
  assert((await page.locator('#rule-compare').textContent()).includes('0x0002'));
  assert.equal(configurations.mdio.whitelist[4].mask,0);
  await screenshot('07-policy-draft');
  // A silent board must be detected even while a configuration draft is open.
  await idle();versionMode='silent';
  await checkConnection(false);
  assert.equal(await page.locator('[data-rule-field="mask"]').inputValue(),'0x0002');
  versionMode='normal';await checkConnection(true);
  assert.equal(await page.locator('[data-rule].changed').count(),1);
  // An HTTP operation error is not a loss of the network connection.
  const failedVersion=page.waitForResponse(res=>res.url().endsWith('/api/version')&&res.status()===503);
  versionMode='error';await failedVersion;await idle();await checkConnection(true);
  const invalidVersion=page.waitForResponse(res=>res.url().endsWith('/api/version')&&res.status()===200);
  versionMode='malformed';await invalidVersion;await idle();await checkConnection(true);versionMode='normal';
  await page.locator('[data-rule-field="mask"]').fill('0x10000');
  assert(await page.locator('[data-apply]').isDisabled());
  await page.locator('[data-discard]').click();
  await page.locator('[data-delete="whitelist:0"]').click();
  assert.equal(await page.locator('[data-rule].changed').count(),0);
  assert.equal(await page.locator('#deleted-rules li').count(),1);
  await page.locator('[data-discard]').click();
  // A conflicting external update must preserve the browser draft.
  await page.locator('[data-delete="whitelist:4"]').click();conflict=true;
  await page.locator("[data-apply]").click();await page.getByText(/Конфигурация изменена другим клиентом/).waitFor();
  assert.equal(configurations.mdio.whitelist.length,5);conflict=false;
  await page.locator("[data-discard]").click();
  await page.locator('[data-tab="registers"]').click();await page.getByRole("heading",{name:"Регистр 0x02"}).waitFor();
  const busReads = () => calls.filter(([method,path,q])=>method==='PUT'&&path==='/api/ui/bus'&&q.action==='read').length;
  const beforeRead=busReads();
  assert.equal(await page.locator('[data-save]').count(),0);
  assert.equal(await page.locator('.bit span.set').count(),3);
  assert(await page.locator('[data-write]').isDisabled());
  await page.locator('[data-register="3"]').click();
  await page.locator('[data-register="3"]').press('ArrowUp');
  await page.getByRole('heading',{name:'Регистр 0x02'}).waitFor();
  assert.equal(busReads(),beforeRead);
  await page.locator('[data-snapshot]').click();await page.getByRole('heading',{name:'Регистр 0x02'}).waitFor();
  assert.equal(busReads(),beforeRead);
  await page.locator('[data-read]').click();await page.locator('#reg-result').filter({hasText:'0x0007 (7)'}).waitFor();
  assert.equal(busReads(),beforeRead+1);
  await screenshot('06-mdio-registers');
  await page.locator('[data-register="0"]').click();
  await page.locator('#val').fill('0x1140');assert(await page.locator('[data-write]').isEnabled());
  const writesBefore=calls.filter(([method,path,q])=>method==='PUT'&&q.action==='write').length;
  await page.locator('[data-write]').click();
  assert.equal(calls.filter(([method,path,q])=>method==='PUT'&&q.action==='write').length,writesBefore);
  await page.locator('#modal[open] #modal-submit').click();
  await page.locator('#reg-result').filter({hasText:'Кадр передан'}).waitFor();
  assert.equal(calls.filter(([method,path,q])=>method==='PUT'&&q.action==='write'&&q.confirm==='true').length,writesBefore+1);
  await page.locator('[data-tab="autopoll"]').click();await page.locator('[data-regs]').fill("2, 2");
  assert(await page.locator("[data-apply]").isDisabled());
  await page.locator('[data-regs]').fill("0,1,2,3,4,5,17,31");
  await page.locator('[data-nav="i2c"]').click();await page.locator('.device-name').filter({hasText:"axp15060"}).waitFor();
  await page.locator('[data-tab="policy"]').click();assert.equal(await page.getByRole("columnheader",{name:"Маска",exact:true}).count(),0);
  await page.locator('[data-tab="registers"]').click();
  assert.equal(await page.locator('.register-table .badge.ok').count(),0);
  await page.locator('[data-register="19"]').click();await page.locator('#val').fill('16');
  assert(await page.locator('[data-write]').isEnabled());
  await page.locator('#val').fill('17');assert(await page.locator('[data-write]').isDisabled());
  await page.locator('[data-read]').click();await page.locator('#reg-result').filter({hasText:'0x07 (7)'}).waitFor();
  assert.equal(await page.locator('.register-table .badge.ok').count(),1);
  await page.locator('[data-nav="files"]').click();await page.getByRole("button",{name:"roundtrip.json",exact:true}).waitFor();
  await page.getByRole("button",{name:"roundtrip.json",exact:true}).click();await page.locator('pre.file-preview').waitFor();await screenshot("03-files");
  assert.equal(await page.locator('[data-volume]').count(),0);
  assert.equal(await page.locator('[data-volume-nav="sd-pl"][aria-current="page"]').count(),1);
  await page.getByRole("button",{name:"Создать каталог"}).click();await page.locator('#modal [name="name"]').fill("web-test");
  await page.getByRole("button",{name:"Создать",exact:true}).click();await page.getByRole("button",{name:"web-test",exact:true}).waitFor();
  await page.locator('#modal').waitFor({state:"hidden"});
  await page.locator('#upload-file').setInputFiles({name:"hello.txt",mimeType:"text/plain",buffer:Buffer.from("hello")});
  await page.locator('#modal[open] #modal-submit').click();await page.getByRole("button",{name:"hello.txt",exact:true}).waitFor();
  await page.getByRole("button",{name:"hello.txt",exact:true}).click();await page.getByRole("button",{name:"Редактировать текст",exact:true}).click();
  await page.locator('#modal textarea').fill("edited");await page.locator('#modal [name="confirm"]').check();
  await page.getByRole("button",{name:"Записать изменения"}).click();await page.locator('#modal').waitFor({state:"hidden"});
  assert.equal(files.get("sd-pl:/hello.txt").toString(),"edited");
  // Repeated selection must not leave a toolbar action bound to an old object.
  await page.getByRole('button',{name:'Firmware.tif',exact:true}).click();
  await page.getByRole('button',{name:'hello.txt',exact:true}).click();
  await page.getByRole('button',{name:'Копировать',exact:true}).click();
  await page.locator('#modal [name="volume"]').selectOption('flash');
  await page.locator('#modal [name="path"]').fill('/hello-copy.txt');
  await page.locator('#modal[open] #modal-submit').click();await page.locator('#modal').waitFor({state:'hidden'});
  assert.equal(files.get('flash:/hello-copy.txt').toString(),'edited');
  await page.getByRole("button",{name:"hello.txt",exact:true}).click();await page.getByRole("button",{name:"Переименовать",exact:true}).click();
  await page.locator('#modal [name="name"]').fill("renamed.txt");await page.locator('#modal-submit').click();
  await page.getByRole("button",{name:"renamed.txt",exact:true}).waitFor();
  await page.getByRole("button",{name:"renamed.txt",exact:true}).click();await page.getByRole("button",{name:"Удалить",exact:true}).click();
  await page.locator('#modal-submit').click();await page.locator('#modal').waitFor({state:"hidden"});assert(!files.has("sd-pl:/renamed.txt"));
  // Untrusted filenames are text, never markup.
  files.set('sd-pl:/<img src=x onerror=alert(1)>.txt',Buffer.from("x"));
  await page.locator('[data-refresh]').click();await page.getByRole("button",{name:'<img src=x onerror=alert(1)>.txt',exact:true}).waitFor();
  assert.equal(await page.locator('#content img').count(),0);
  await page.locator('[data-volume-nav="flash"]').click();await page.getByRole('button',{name:'hello-copy.txt',exact:true}).waitFor();
  assert.equal(await page.locator('#sidebar a.active').count(),1);
  await page.locator('[data-nav="files"]').click();await page.getByRole('button',{name:'test1',exact:true}).waitFor();
  await page.locator('.path-form input').fill('sd-pl:/test1');await page.locator('.path-form input').press('Enter');
  await page.waitForURL(/path=%2Ftest1/);await page.locator('[data-up]').click();
  await page.getByRole('button',{name:'roundtrip.json',exact:true}).waitFor();
  await page.locator('[data-nav="system"]').click();await page.getByRole("heading",{name:"Система",exact:true}).waitFor();await screenshot("04-system");
  await page.locator('[data-system-nav="runtime"]').click();await page.locator('[data-section="runtime"]:not(.hidden)').waitFor();
  assert.equal(await page.locator('#sidebar a.active').count(),1);
  // A previous dialog's queued close event must not clear the next submit handler.
  assert(await page.evaluate(async()=>{
    const {ask}=await import('/js/ui.js');
    const first=ask({title:'Первый диалог',body:'<p>Проверка закрытия</p>'});
    await Promise.resolve();document.getElementById('modal').close();
    const second=ask({title:'Второй диалог',body:'<p>Проверка подтверждения</p>'});
    await first;await Promise.resolve();
    document.getElementById('modal-form').requestSubmit();
    return await second;
  }));
  // Browser network events are immediate and do not close a dialog or lose input.
  await page.evaluate(async()=>{
    const {ask}=await import('/js/ui.js');
    window.connectionDialog=ask({title:'Проверка связи',body:'<label>Черновик<input name="connection-draft"></label>'});
  });
  await page.locator('#modal [name="connection-draft"]').fill('Сохранить текст');
  await page.context().setOffline(true);await checkConnection(false,1000);
  await page.context().setOffline(false);await checkConnection(true);
  assert.equal(await page.locator('#modal [name="connection-draft"]').inputValue(),'Сохранить текст');
  await page.locator('#modal [data-dismiss]').first().click();
  await page.locator('#modal').waitFor({state:'hidden'});
  // Monitoring must not queue probes behind a long operation or external download.
  await idle();
  await page.evaluate(async()=>{
    const api=await import('/js/api.js');
    window.busyOperation=api.queue(()=>new Promise(resolve=>{window.releaseBusyOperation=resolve;}));
  });
  await page.waitForFunction(()=>typeof window.releaseBusyOperation==='function');
  const busyVersions=versionCalls();await page.waitForTimeout(1200);
  assert.equal(versionCalls(),busyVersions);
  await page.evaluate(()=>window.releaseBusyOperation());await idle();
  await page.evaluate(async()=>(await import('/js/api.js')).pausePolling(60000));
  const pausedVersions=versionCalls();await page.waitForTimeout(1200);
  assert.equal(versionCalls(),pausedVersions);
  const resumedVersion=page.waitForResponse(res=>res.url().endsWith('/api/version'));
  await page.evaluate(async()=>(await import('/js/api.js')).pausePolling(0));
  await resumedVersion;await checkConnection(true);
  await page.setViewportSize({width:390,height:844});
  await page.locator('#menu-toggle').click();
  await page.waitForFunction(()=>document.getElementById('sidebar').getBoundingClientRect().left>=-1);
  await checkSidebarArtwork();
  await page.locator('[data-nav="overview"]').click();await page.getByRole("heading",{name:"Состояние подсистем",exact:true}).waitFor();
  await page.waitForFunction(()=>document.getElementById("sidebar").getBoundingClientRect().right<=1);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));await screenshot("05-mobile");
  assert.deepEqual(errors,[]);
  assert(calls.some(([method,path,q])=>method==="PUT"&&path==="/api/ui/bus"&&q.action==="save"));
  console.log("Browser UI tests passed: live online/offline status, timeout and recovery, no overlapping probes, object tree, icon classes, edge-to-edge waves, short-screen navigation, stale snapshots, rule diff, explicit save, conflict, passive register selection, confirmed writes, file preview, cross-volume copy, toolbar selection, escaping, mobile layout");
}catch(error){
  console.error("Browser errors:",errors,"Recent requests:",calls.slice(-8));
  console.error("Modal:",await page.locator('#modal').textContent());
  await screenshot("failure");throw error;
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
