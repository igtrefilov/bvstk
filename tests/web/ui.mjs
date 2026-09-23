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
const additional = {i2c:[],mdio:[]};
// Device files are v2; the existing web API intentionally exposes its v1
// projection. Mirror that adapter here, not a new browser-side file parser.
function apiProjection(p) {
  assert.equal(p.schema_version,2);
  const bus=p.transport.bus, mdio=bus==='mdio', options=p.protocol.options;
  assert.equal(p.protocol.driver,`bvstk_${bus}_bridge`);
  const regs=new Map(p.resources.map(r=>[r.id,Number(r.address)]));
  const reg=id=>{assert(regs.has(id));return regs.get(id);};
  const write=p.policy.write;
  const rules=list=>list.map(r=>({reg:reg(r.resource),val:Number(r.match.value),...(mdio?{mask:Number(r.match.mask)}:{})}));
  const active=rules(write.rules),inactive=rules(write.inactive_rules||[]);
  return {name:p.id,reg_count:p.resources.length,policy:write.mode,
    whitelist:write.mode==='whitelist'?active:inactive,blacklist:write.mode==='blacklist'?active:inactive,
    settings:p.settings.cache_seed.map(s=>({reg:reg(s.resource),val:Number(s.value)})),
    ...(mdio?{phy_addr:Number(p.transport.address),autopoll_enabled:options.autopoll_enabled,
      autopoll_regs:options.autopoll_order.map(reg),autopoll_reg_delay_ms:options.autopoll_reg_delay_ms,
      autopoll_cycle_delay_ms:options.autopoll_cycle_delay_ms}:
      {addr_7b:Number(p.transport.address),max_value_code:Number(options.max_write_value)})};
}
for (const [bus,name] of [['i2c','pca9536'],['mdio','dp83848c'],['mdio','ksz8081rna']]) {
  additional[bus].push(apiProjection(JSON.parse(await readFile(new URL(`../../configs/${bus}/${name}.json`,import.meta.url),'utf8'))));
}
const presence = {i2c:'responding',mdio:'responding'};
const revision={mdio:1,i2c:1},saved={mdio:true,i2c:true};
const calls=[]; let conflict=false, failRtos=false, mdioListState="devices", versionMode="normal";
const observations = {i2c:{events:[],telemetry:[]},mdio:{events:[],telemetry:[]}};
let observeFail = false, observeLoss = "0", observeUptime = 100000;
function observed(bus, channel, data) {
  const rows = observations[bus][channel];
  rows.push({id:String(rows.length+1),time_us:rows.length*10000+5000000,type:258,status:0,source:1,
    operation:0,address:bus==='mdio'?1:54,reg:2,value:7,...data});
}
function telemetryAvailable(bus,address) {
  const profiles = [configurations[bus],...additional[bus]];
  const profile = profiles.find(p => (bus === 'mdio' ? p.phy_addr : p.addr_7b) === address);
  if (!profile) return false;
  return (profile === configurations[bus] ? presence[bus] : bus === 'i2c' ? 'unknown' : 'unavailable') === 'responding';
}
const files = new Map([["sd-pl:/test1",null],["sd-pl:/roundtrip.json",Buffer.from('{"example":true}\n')],["sd-pl:/Firmware.tif",Buffer.alloc(223423)]]);
const server=http.createServer(async(req,res)=>{
  const u=new URL(req.url,"http://local"),q=Object.fromEntries(u.searchParams),chunks=[];
  for await(const chunk of req)chunks.push(chunk);
  const body=Buffer.concat(chunks);
  const reply=(value,status=200)=>{res.writeHead(status,{"Content-Type":"application/json"});res.end(JSON.stringify(value));};
  const error=(message,status)=>reply({ok:false,error:message},status);
  calls.push([req.method,u.pathname,q,body.length]);
  if(req.method==='PUT' && (u.pathname==='/api/ui/files' || (u.pathname==='/api/ui/bus' && q.action!=='apply'))) {
    assert.equal(body.length,0,'Query-only commands must not leave an ignored body in the board receive queue');
  }
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
    const b=q.bus,profiles=[configurations[b],...additional[b]],c=q.name?profiles.find(p=>p.name===q.name):configurations[b];
    if(!c)return error('unknown profile',404);
    const state=profile=>profile===configurations[b]?presence[b]:b==='i2c'?'unknown':'unavailable';
    if(req.method==="GET"){
      if(!q.name&&b==="mdio"&&mdioListState!=="devices")return reply({ready:mdioListState==="empty",devices:[]});
      if(!q.name)return reply({ready:true,devices:profiles.map(p=>({name:p.name,address:b==="mdio"?p.phy_addr:p.addr_7b,policy:p.policy,available:state(p)==='responding',presence:state(p)}))});
      const cache=Array.from({length:c.reg_count},(_,reg)=>({reg,val:reg===2?7:reg===3?49393:0,valid:reg<8&&state(c)==='responding',source:b==='mdio'?"PHY":"I2C",updated_ms:100}));
      if(q.action==="cache")return reply({cache,stats:{reads:128,writes:3,denied:0,errors:0,observed:3}});
      return reply({config:c,revision:String(revision[b]),saved:saved[b],available:state(c)==='responding',presence:state(c),file_name:c.name+".json",cache,stats:{reads:128,writes:3,denied:0,errors:0,observed:3}});
    }
    if(q.action==="apply"||q.action==="save"){
      if(conflict||q.revision!==String(revision[b]))return error("configuration changed",409);
      if(q.action==="apply"){configurations[b]=JSON.parse(body);revision[b]++;saved[b]=false;}
      else saved[b]=true;
    }
    return reply(q.action==="read"?{ok:true,reg:Number(q.reg),val:7}:{ok:true});
  }
  if(u.pathname==="/api/ui/observe"){
    if(observeFail)return error('journal unavailable',503);
    const rows=observations[q.bus][q.channel],after=Number(q.after||0),reset=after>rows.length;
    const scan=rows.slice(reset?0:after,(reset?0:after)+32),last=scan.at(-1)?.id||String(rows.length);
    const lost=observeLoss;observeLoss="0";
    return reply({ok:true,next:last,head:String(rows.length),lost,reset,more:Number(last)<rows.length,
      uptime_ms:observeUptime,capacity:256,records:q.channel === 'telemetry' && !telemetryAvailable(q.bus,Number(q.address)) ? [] : scan.filter(r=>r.address===Number(q.address))});
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
async function checkNavigationGroup(name) {
  const summary=page.locator('.nav-branch > summary').filter({hasText:name});
  const branch=summary.locator('..'), url=page.url();
  const active=await page.locator('#sidebar [aria-current="page"]').getAttribute('href');
  assert(await branch.evaluate(el=>el.open));
  await summary.click();
  assert.equal(await branch.evaluate(el=>el.open),false,name+' must collapse when its label is clicked');
  assert.equal(page.url(),url);
  assert.equal(await page.locator('#modal[open]').count(),0);
  // A navigation metadata refresh must not reopen an explicitly collapsed branch.
  await page.evaluate(async()=>{await (await import('/js/api.js')).busList('mdio');});
  assert.equal(await branch.evaluate(el=>el.open),false,name+' must stay collapsed after a refresh');
  assert.equal(await page.locator('#sidebar [aria-current="page"]').getAttribute('href'),active);
  await summary.press('Enter');
  assert(await branch.evaluate(el=>el.open));
  await summary.press('Space');
  assert.equal(await branch.evaluate(el=>el.open),false);
  await summary.click();
  assert(await branch.evaluate(el=>el.open));
  assert.equal(page.url(),url);
}
async function checkPolicyListLayout() {
  const layout=await page.locator('#policy-workspace').evaluate(root=>{
    const buttons=[...root.querySelectorAll('[data-list]')];
    const box=el=>{const r=el.getBoundingClientRect();return {left:r.left,right:r.right,top:r.top,bottom:r.bottom,width:r.width,height:r.height};};
    return {buttons:buttons.map(el=>({...box(el),clientWidth:el.clientWidth,scrollWidth:el.scrollWidth})),
      card:box(buttons[0].closest('article')),add:box(root.querySelector('[data-add-rule]'))};
  });
  const [white,black]=layout.buttons;
  assert.equal(layout.buttons.length,2);
  assert(Math.abs(white.width-black.width)<=1,'Policy list tabs must have equal widths');
  assert(white.width>=layout.card.width*.45,'Policy list tabs must fill the rule panel');
  assert(Math.abs(white.left-layout.card.left)<=2&&Math.abs(black.right-layout.card.right)<=2);
  assert(white.height>=40&&Math.abs(white.top-black.top)<=1);
  assert(layout.add.top>=white.bottom+8,'Add rule belongs on a separate toolbar');
  for(const tab of layout.buttons)assert(tab.scrollWidth<=tab.clientWidth+1,'Policy list label must not be clipped');
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Policy page must fit the viewport');
}
async function checkDeviceIcon(name,ready) {
  const link=page.locator(`[data-device-name="${name}"]`), dot=link.locator('.device-status');
  assert.equal(await link.getAttribute('data-device-ready'),String(ready));
  assert.equal(await dot.evaluate(el=>el.classList.contains('ready')),ready);
  assert.equal(await dot.evaluate(el=>getComputedStyle(el).fill),ready?'rgb(98, 245, 157)':'rgb(154, 171, 186)');
  assert.equal(await link.locator('svg').evaluate(el=>el.getBoundingClientRect().width),25);
  assert.equal(await dot.getAttribute('r'),'2');
  assert(await dot.evaluate(el=>{
    const point=el.getBBox(), chip=el.ownerSVGElement.querySelector('rect').getBBox();
    const pointCenter={x:point.x+point.width/2,y:point.y+point.height/2};
    const chipCenter={x:chip.x+chip.width/2,y:chip.y+chip.height/2};
    return Math.abs(pointCenter.x-chipCenter.x)<.01 && Math.abs(pointCenter.y-chipCenter.y)<.01 &&
      point.x>chip.x && point.y>chip.y && point.x+point.width<chip.x+chip.width && point.y+point.height<chip.y+chip.height;
  }),'Readiness point belongs at the center of the chip');
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
  const subsystemStates = await page.locator('table[aria-label="Состояние подсистем"] tbody td:nth-child(4)').allTextContents();
  assert(subsystemStates.length > 0);
  assert(subsystemStates.every(state => ['Готово','Не готово'].includes(state.trim())),`Unexpected subsystem state: ${subsystemStates.join(', ')}`);
  assert.equal(await page.locator('[data-subsystem="i2c:axp15060"] td').nth(3).textContent(),'Готово');
  assert.equal(await page.locator('[data-subsystem="i2c:pca9536"] td').nth(3).textContent(),'Не готово');
  assert.equal(await page.locator('[data-subsystem="mdio:lan8720"] td').nth(3).textContent(),'Готово');
  assert.equal(await page.locator('[data-subsystem="mdio:dp83848c"] td').nth(3).textContent(),'Не готово');
  assert.equal(await page.locator('[data-subsystem="i2c:pca9536"] .badge.offline').count(),1);
  assert.equal(await page.locator('[data-subsystem="mdio:dp83848c"] .badge.offline').count(),1);
  assert.deepEqual(await page.locator('.nav-branch > summary').allTextContents(),['Подсистемы','Файлы','Система']);
  await page.locator('[data-device-name="lan8720"]').waitFor();
  await checkConnection(true);
  const glyph = selector => page.locator(selector+' svg').innerHTML();
  assert.equal(await glyph('[data-nav="i2c"]'),await glyph('[data-nav="mdio"]'));
  assert.equal(await glyph('[data-device-name="axp15060"]'),await glyph('[data-device-name="lan8720"]'));
  assert.notEqual(await glyph('[data-nav="i2c"]'),await glyph('[data-device-name="lan8720"]'));
  assert.equal(await page.locator('#sidebar a.active').count(),1);
  assert.equal(await page.locator('[data-device-tree="i2c"] a').count(),2);
  assert.equal(await page.locator('[data-device-tree="mdio"] a').count(),3);
  for(const name of ['axp15060','lan8720'])await checkDeviceIcon(name,true);
  for(const name of ['pca9536','dp83848c','ksz8081rna'])await checkDeviceIcon(name,false);
  assert.equal(await page.locator('[data-subsystem="mdio:lan8720"] td').nth(2).textContent(),'0x01');
  assert(await page.locator('[data-icon-name="settings"] path').evaluate(el=>{
    const values=el.getAttribute('d').match(/-?\d+(?:\.\d+)?/g).map(Number),points=[];
    for(let i=0;i<values.length;i+=2)points.push([values[i]-12,values[i+1]-12]);
    return points.length===32 && points.every(([x,y])=>points.some(([px,py])=>
      Math.abs(px-(x-y)/Math.sqrt(2))<.002 && Math.abs(py-(x+y)/Math.sqrt(2))<.002));
  }),'Gear teeth must be symmetric under a 45 degree rotation');
  assert.equal(await page.evaluate(async()=>(await import('/js/ui.js')).deviceState('i2c',{available:true}).ready),false);
  presence.i2c='unknown';await page.evaluate(async()=>{await (await import('/js/api.js')).busList('i2c');});
  await checkDeviceIcon('axp15060',false);
  presence.i2c='responding';await page.evaluate(async()=>{await (await import('/js/api.js')).busList('i2c');});
  await checkDeviceIcon('axp15060',true);
  for(const group of ['Подсистемы','Файлы','Система'])await checkNavigationGroup(group);
  assert.equal(await page.locator('#sidebar').evaluate(el=>getComputedStyle(el).backgroundColor),'rgb(48, 57, 65)');
  await checkSidebarArtwork();
  assert.equal(await page.getByText('Тракт управления',{exact:false}).count(),0);
  await page.locator('[data-subsystem="i2c:axp15060"]').click();
  await page.locator('#overview-inspector').getByText('axp15060',{exact:true}).waitFor();
  assert.equal(await page.locator('.device-meta .badge').textContent(),'Готово');
  await page.locator('[data-subsystem="mdio:lan8720"]').click();
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
  await screenshot("01-overview");
  for(const [name,address] of [['pca9536','0x41'],['dp83848c','0x02'],['ksz8081rna','0x03']]) {
    await page.locator(`[data-device-name="${name}"]`).click();
    await page.locator('.device-name').filter({hasText:name}).waitFor();
    assert.equal(await page.locator('.device-meta>span').first().textContent(),'Адрес '+address);
    assert.equal(await page.locator('.device-meta .badge.ok').count(),0);
    assert.equal(await page.locator('.device-meta .badge.offline').count(),1);
    assert.equal(await page.locator('.device-meta .badge').textContent(),'Не готово');
  }
  await page.locator('[data-nav="overview"]').click();
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
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
  await page.locator('[data-subsystem="mdio:"]').getByText('Не готово',{exact:true}).waitFor();
  assert.equal(detailReads(),beforeUnavailable);
  mdioListState="empty";await page.locator('[data-refresh]').click();
  await page.locator('[data-subsystem="mdio:"]').getByText('Не готово',{exact:true}).waitFor();
  await page.locator('#overview-inspector').getByText('Нет настроенных устройств',{exact:true}).waitFor();
  assert.equal(detailReads(),beforeUnavailable);
  mdioListState="devices";await page.locator('[data-refresh]').click();
  await page.locator('#overview-inspector').getByText('Сохранена',{exact:true}).waitFor();
  await page.locator('[data-nav="mdio"]').click();
  await page.locator('.device-name').filter({hasText:"lan8720"}).waitFor();
  assert.equal(await page.locator('.device-meta>span').first().textContent(),'Адрес 0x01');
  assert.equal(await page.locator('.device-meta .badge').textContent(),'Готово');
  const writesBeforeObserve=calls.filter(([method])=>method==='PUT').length;
  observed('mdio','telemetry',{});
  observed('mdio','telemetry',{source:3,reg:3,value:49393});
  observed('mdio','telemetry',{type:257,operation:1,reg:4,value:481});
  observed('mdio','telemetry',{address:2,reg:1,value:65535});
  await page.locator('[data-tab="telemetry"]').click();
  await page.locator('[data-record-id="2"]').waitFor();
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),2);
  assert.equal(await page.locator('[data-record-id="1"] td').nth(6).textContent(),'Успешно');
  assert.equal(await page.locator('[data-record-id="1"]').evaluate(el=>getComputedStyle(el.querySelector('td:last-child')).color),'rgb(40, 122, 67)');
  assert.equal(await page.locator('.save-bar').count(),0);
  await page.locator('[data-live-source]').selectOption('1');
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),1);
  await page.locator('[data-live-reg]').fill('0x04');
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),0);
  await page.locator('[data-live-reg]').fill('');
  await page.locator('[data-live-source]').selectOption('');
  await page.locator('[data-live-pause]').click();
  observed('mdio','telemetry',{type:258,source:4,reg:5,value:0,status:5});
  await page.waitForTimeout(700);
  const pausedObserveRequests=calls.filter(([method,path])=>method==='GET'&&path==='/api/ui/observe').length;
  await page.waitForTimeout(700);
  assert.equal(calls.filter(([method,path])=>method==='GET'&&path==='/api/ui/observe').length,pausedObserveRequests);
  await page.locator('[data-live-state]').filter({hasText:'Приём новых записей приостановлен. Получено строк: 2'}).waitFor();
  assert.equal(await page.locator('[data-record-id="5"]').count(),0);
  await page.locator('[data-live-pause]').click();
  await page.locator('[data-record-id="5"]').waitFor();
  assert.equal(await page.locator('[data-record-id="5"] td').nth(5).textContent(),'—');
  assert.equal(await page.locator('[data-record-id="5"] td').nth(6).textContent(),'Ошибка');
  assert.equal(await page.locator('[data-record-id="5"]').evaluate(el=>getComputedStyle(el.querySelector('td:last-child')).color),'rgb(163, 50, 39)');
  observeLoss='3';
  await page.locator('[data-live-warning]').filter({hasText:'Пропущено в программном журнале шины: 3'}).waitFor();
  await screenshot('09-mdio-telemetry');
  observeFail=true;
  await page.locator('[data-live-state]').filter({hasText:'Обновление недоступно'}).waitFor();
  observeFail=false;
  await page.locator('[data-live-state]').filter({hasText:'Обновляется автоматически'}).waitFor();
  await page.locator('[data-live-clear]').click();
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),0);
  observed('mdio','events',{type:1,operation:1,source:4,reg:4,value:481});
  observed('mdio','events',{type:3,operation:1,source:4,reg:4,value:481,status:3});
  await page.locator('[data-tab="events"]').click();
  await page.locator('[data-record-id="2"]').waitFor();
  assert(await page.locator('[data-live-rows]').textContent().then(t=>t.includes('Ошибка')));
  await screenshot('10-mdio-events');
  await page.locator('[data-device-name="dp83848c"]').click();
  await page.locator('.device-name').filter({hasText:'dp83848c'}).waitFor();
  await page.locator('[data-tab="telemetry"]').click();
  await page.locator('[data-live-rows] .empty').filter({hasText:'Нет подтверждённых взаимодействий'}).waitFor();
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),0);
  observed('i2c','telemetry',{address:65,reg:2,value:255});
  await page.locator('[data-device-name="pca9536"]').click();
  await page.locator('.device-name').filter({hasText:'pca9536'}).waitFor();
  await page.locator('[data-tab="telemetry"]').click();
  await page.locator('[data-live-rows] .empty').filter({hasText:'Нет подтверждённых взаимодействий'}).waitFor();
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),0);
  observed('i2c','telemetry',{type:260,reg:19,value:19});
  observed('i2c','telemetry',{type:262,reg:19,value:16});
  await page.locator('[data-device-name="axp15060"]').click();
  await page.locator('.device-name').filter({hasText:'axp15060'}).waitFor();
  await page.locator('[data-tab="telemetry"]').click();
  await page.locator('[data-live-rows] .empty').filter({hasText:'Нет подтверждённых взаимодействий'}).waitFor();
  assert.equal(await page.locator('[data-live-rows] tr[data-record-id]').count(),0);
  await screenshot('11-i2c-telemetry');
  for(const width of [1280,768,390]) {
    await page.setViewportSize({width,height:992});
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Live view must fit viewport');
  }
  await page.setViewportSize({width:1586,height:992});
  assert.equal(calls.filter(([method])=>method==='PUT').length,writesBeforeObserve,'Observing does not trigger physical operations');
  await page.locator('[data-device-name="lan8720"]').click();
  await page.locator('.device-name').filter({hasText:'lan8720'}).waitFor();
  await page.locator('[data-tab="policy"]').click();
  for(const width of [1920,1586,1280,1024,768,390]) {
    await page.setViewportSize({width,height:992});
    await checkPolicyListLayout();
  }
  await screenshot('08-policy-mobile');
  await page.setViewportSize({width:1586,height:992});
  await page.locator('[data-list="blacklist"]').click();
  assert.equal(await page.locator('[data-list="blacklist"]').getAttribute('aria-pressed'),'true');
  await page.locator('#rules-table').getByText('Список пуст',{exact:true}).waitFor();
  assert(await page.locator('[name="policy"][value="whitelist"]').isChecked());
  assert(await page.locator('[data-apply]').isDisabled());
  await checkPolicyListLayout();
  await page.locator('[data-list="whitelist"]').click();
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
  for(const group of ['Файлы','Система'])await checkNavigationGroup(group);
  assert.equal(await page.locator('[data-rule-field="mask"]').inputValue(),'0x0002');
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
  assert.equal(await page.getByRole('columnheader',{name:'Значение в кэше',exact:true}).count(),1);
  assert.equal(await page.getByRole('columnheader',{name:'Последнее обновление',exact:true}).count(),1);
  assert.equal(await page.getByRole('columnheader',{name:'Достоверность',exact:true}).count(),0);
  assert.equal(await page.getByRole('columnheader',{name:'Источник',exact:true}).count(),0);
  assert.equal(await page.getByRole('columnheader',{name:'Возраст',exact:true}).count(),0);
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
  assert.equal(await page.getByText("Правила совпадают с рабочим снимком RAM",{exact:true}).count(),0);
  assert.equal(await page.getByText("Проверка правила",{exact:true}).count(),0);
  assert.equal(await page.getByText("Разрешать только совпавшие правила",{exact:true}).count(),0);
  await checkPolicyListLayout();
  await page.setViewportSize({width:390,height:844});await checkPolicyListLayout();
  await page.setViewportSize({width:1586,height:992});
  await page.locator('[data-tab="registers"]').click();
  assert.equal(await page.locator('.register-table .badge').count(),0);
  await page.locator('[data-register="19"]').click();await page.locator('#val').fill('16');
  assert(await page.locator('[data-write]').isEnabled());
  await page.locator('#val').fill('17');assert(await page.locator('[data-write]').isDisabled());
  await page.locator('[data-read]').click();await page.locator('#reg-result').filter({hasText:'0x07 (7)'}).waitFor();
  assert.match(await page.locator('[data-register="19"] td').nth(2).textContent(),/^T\+\d{2}:\d{2}:\d{2}\.\d{3}$/);
  await page.locator('[data-volume-nav="sd-pl"]').click();await page.getByRole("button",{name:"roundtrip.json",exact:true}).waitFor();
  await checkNavigationGroup('Файлы');
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
  await page.locator('[data-volume-nav="sd-pl"]').click();await page.getByRole('button',{name:'test1',exact:true}).waitFor();
  await page.locator('.path-form input').fill('sd-pl:/test1');await page.locator('.path-form input').press('Enter');
  await page.waitForURL(/path=%2Ftest1/);await page.locator('[data-up]').click();
  await page.getByRole('button',{name:'roundtrip.json',exact:true}).waitFor();
  await page.locator('[data-system-nav="network"]').click();await page.getByRole("heading",{name:"Система",exact:true}).waitFor();await screenshot("04-system");
  await checkNavigationGroup('Система');
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
  assert.equal(await page.locator('.device-status.ready').count(),0);
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
  for(const group of ['Файлы','Система']) {
    await checkNavigationGroup(group);
    assert.equal(await page.locator('#menu-toggle').getAttribute('aria-expanded'),'true');
  }
  await page.locator('[data-nav="overview"]').click();await page.getByRole("heading",{name:"Состояние подсистем",exact:true}).waitFor();
  await page.waitForFunction(()=>document.getElementById("sidebar").getBoundingClientRect().right<=1);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));await screenshot("05-mobile");
  assert.deepEqual(errors,[]);
  assert(calls.some(([method,path,q])=>method==="PUT"&&path==="/api/ui/bus"&&q.action==="save"));
  console.log("Browser UI tests passed: telemetry and write events for I2C/MDIO, independent device filters, live updates, display pause with continued reception, software-loss notices, failed-read values, read-only observation, reconnect and mobile layout; existing profiles, navigation, policies, registers, file operations and online status");
}catch(error){
  console.error("Browser errors:",errors,"Recent requests:",calls.slice(-8));
  console.error("Modal:",await page.locator('#modal').textContent());
  await screenshot("failure");throw error;
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
