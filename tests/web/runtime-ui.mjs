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
for (const [bus,name,directory=name] of [['i2c','pca9555','pca9545a/pca9555'],['mdio','dp83848c'],['mdio','ksz8081rna']]) {
  additional[bus].push(apiProjection(JSON.parse(await readFile(new URL(`../../configs/${bus}/${directory}/${name}.json`,import.meta.url),'utf8'))));
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
  if(u.pathname==="/api/runtime")return failRtos ? error("Runtime unavailable",503) : reply({schema_version:1,os:process.env.BVSTK_TEST_OS || "freertos",uptime_ms:759000,capabilities:{},os_details:process.env.BVSTK_TEST_OS === "neutrino" ? {pid:42,online_cpus:2} : {heap_free:396697,heap_min_ever:367718,tick_rate_hz:100}});
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
const page=await browser.newPage({viewport:{width:1440,height:1000}});const errors=[];
page.on("pageerror",e=>errors.push(e.message));
try {
  const base=`http://127.0.0.1:${server.address().port}/`;
  await page.goto(base);
  await page.getByRole("heading",{name:"Состояние подсистем",exact:true}).waitFor();
  assert.equal((await page.locator('[data-system-nav="runtime"]').innerText()).trim(),"Среда исполнения");
  await page.goto(base+"#system?tab=runtime");
  await page.getByRole("heading",{name:"Среда исполнения",exact:true}).waitFor();
  const text=await page.locator('[data-section="runtime"]').innerText();
  if(process.env.BVSTK_TEST_OS==="neutrino") {
    assert(text.includes("Нейтрино")&&text.includes("PID сервера")&&text.includes("42"));
    assert(!text.includes("Частота тиков")&&!text.includes("Свободная память"));
  } else assert(text.includes("FreeRTOS")&&text.includes("Свободная память")&&text.includes("100 Гц"));
  assert(!text.includes("undefined"));
  if(output) await page.screenshot({path:resolve(output,"runtime.png"),fullPage:true});
  for(const route of ["#i2c?name=axp15060","#mdio?name=lan8720","#files?volume=flash"]) {
    await page.goto(base+route);await page.waitForFunction(()=>!document.querySelector("#content .loading"));
    assert(!await page.getByRole("heading",{name:"Не удалось загрузить данные"}).count());
  }
  assert.equal(calls.some(c=>c[1]==="/api/rtos"),false);
  assert.deepEqual(errors,[]);
  console.log("Common runtime/device/file UI passed for "+(process.env.BVSTK_TEST_OS||"freertos"));
} finally { await browser.close();await new Promise(resolve=>server.close(resolve)); }
