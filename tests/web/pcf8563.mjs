import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import vm from "node:vm";
const source = await readFile(new URL("../../configs/i2c/pca9545a/pcf8563/view.js",import.meta.url),"utf8");
assert(Buffer.byteLength(source)<=65536);
const sandbox={window:{}};vm.runInNewContext(source,sandbox);
const {decode}=sandbox.window.bvstkView;
const words=[0,3,0x52,0x38,0x14,0x22,4,0x10,0x26,0x30,7,0x80,0x80,0x80,0x82,45];
const sample=()=>({device:{presence:"responding"},uptime_ms:2200,
  cache:words.map((val,reg)=>({reg,val,valid:true,source:"I2C",updated_ms:2000,read_id:7,read_duration_ms:5}))});
let s=sample(),d=decode(s);
assert(d.combined&&d.coherent&&!d.stale);assert.equal(d.count,16);
assert.equal(JSON.stringify(d.parts),JSON.stringify([14,38,52,22,10,26]));
assert.equal(d.alarmEnabled,2);assert(d.alarmKnown&&!d.alarmInvalid);
assert.equal(d.aie,true);assert.equal(d.af,false);assert.equal(d.te,true);assert.equal(d.td,2);
// Same timestamp cannot turn separate transactions into a coherent calendar.
for(const e of s.cache){delete e.read_id;delete e.read_duration_ms;}
assert(!decode(s).coherent);
s=sample();s.cache[4].read_id=8;assert(!decode(s).combined);
s=sample();s.cache[4].updated_ms++;assert(!decode(s).combined);
s=sample();s.cache.forEach(e=>e.read_duration_ms=1000);assert(!decode(s).combined);
s.cache.forEach(e=>e.read_duration_ms=999);assert(decode(s).combined);
for(const invalid of [0,-1,1.5,0x100000000]) {
  s=sample();s.cache.forEach(e=>e.read_id=invalid);assert(!decode(s).coherent);
}
for(const change of [{valid:false},{source:"settings"},{source:"unknown"},{val:256},{val:-1},{updated_ms:NaN},{updated_ms:-1}]) {
  s=sample();Object.assign(s.cache[2],change);d=decode(s);
  assert.equal(d.values[2],null);assert(!d.combined);
}
s=sample();s.cache.push({...s.cache[2]});assert.equal(decode(s).values[2],null);
assert.equal(decode().count,0);assert.equal(decode().vl,null);
s=sample();s.cache=s.cache.slice(0,1);d=decode(s);
assert.equal(d.count,1);assert(!d.hasCalendar&&!d.alarmKnown);assert.equal(d.te,null);
// Real zeroes and 0xff are decoded by field rules, never treated as missing.
s=sample();s.cache[2].val=0;assert.equal(decode(s).parts[2],0);
s.cache[2].val=0xff;assert.equal(decode(s).vl,true);assert(!decode(s).combined);
const ranges=[[2,0x7f,0,59],[3,0x7f,0,59],[4,0x3f,0,23],[5,0x3f,1,31],[7,0x1f,1,12],[8,0xff,0,99]];
for(const [reg,mask,min,max] of ranges)for(let value=0;value<256;value++) {
  s=sample();s.cache[reg].val=value;
  const raw=value&mask,number=(raw>>4)*10+(raw&15);
  const bad=(raw>>4)>9||(raw&15)>9||number<min||number>max;
  const decoded=decode(s);
  if(bad)assert(decoded.invalidFields.includes(reg),"Invalid BCD accepted: "+reg+" "+value);
}
s=sample();s.cache[5].val=0x31;s.cache[7].val=0x04;assert(!decode(s).combined);
s.cache[5].val=0x29;s.cache[7].val=0x02;s.cache[8].val=0x26;assert(!decode(s).combined);
s.cache[8].val=0x24;assert(decode(s).combined);s.cache[8].val=0;assert(decode(s).combined);
s=sample();s.cache[6].val=7;assert(!decode(s).combined);
s=sample();s.cache[7].val=0x90;assert.equal(decode(s).parts[4],10);
s=sample();s.cache[2].val|=0x80;d=decode(s);assert(d.vl&&d.combined);
s.cache[0].val=0x20;assert(decode(s).stop);
s.cache[0].val=0x88;d=decode(s);assert(d.test1&&d.testc);
s.cache[1].val=0x1e;d=decode(s);assert(d.af&&d.tf&&d.aie&&d.pulse&&!d.tie);
s=sample();s.cache[9].val=0x7a;assert(decode(s).alarmInvalid);
s.cache[9].val=0xfa;assert(!decode(s).alarmInvalid,"Disabled alarm comparisons ignore their BCD payload");
for(let r=9;r<=12;r++)s.cache[r].val=0xff;
assert.equal(decode(s).alarmEnabled,0);assert(!decode(s).alarmInvalid);
s=sample();s.uptime_ms=7001;d=decode(s);assert(d.stale&&d.combined);
s=sample();s.stale=true;assert(decode(s).stale);
s=sample();s.device.presence="unavailable";assert(decode(s).disconnected);
s=sample();s.uptime_ms=1999;assert(!decode(s).stale,"HTTP clock can precede a read in the response");
// No state retained across a PS reboot, including old coherent calendar.
decode(sample());assert.equal(decode({device:{presence:"responding"},uptime_ms:50,cache:[]}).count,0);
console.log("PCF8563: BCD/calendar validation, physical sources, read coherence, VL/STOP, alarm, flags and stale snapshots passed");
