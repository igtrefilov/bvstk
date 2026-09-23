import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
const source = await readFile(new URL("../../web/assets/js/register-map.js",import.meta.url));
const {validateRegisterMap,decodeRegisterMap} = await import("data:text/javascript;base64,"+source.toString("base64"));
const bytes = await readFile(new URL("../../configs/i2c/axp15060/register_map.json",import.meta.url));
const map = JSON.parse(bytes), compiled = validateRegisterMap(map,{bus:"i2c",regCount:74});
const profile = JSON.parse(await readFile(new URL("../../configs/i2c/axp15060/axp15060.json",import.meta.url)));
assert(bytes.length <= 65536, "map must fit the deployed asset limit");
assert.equal(profile.register_map,"register_map.json");
assert.equal(profile.view,"view.js");
assert.equal(profile.resources.length,74);
assert.equal(map.identity,undefined,"the datasheet does not document a readable chip ID");
assert.equal(map.registers.length,74);
assert.equal(compiled.fields.size,190);
assert.deepEqual(map.registers.filter(r=>!r.reserved).map(r=>Number(r.address)),[
  0,4,5,6,7,16,17,18,19,20,21,22,23,24,25,26,27,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,49,50,54,64,65,72,73
]);
const entry=(reg,val,extra={})=>({reg,val,valid:true,source:"I2C",updated_ms:123456,...extra});
const field=(ref,reg,val,extra={})=>decodeRegisterMap(compiled,[entry(reg,val,extra)]).fields.get(ref);
const summary=cache=>Object.fromEntries(decodeRegisterMap(compiled,cache).summary.map(x=>[x.id,x]));

// Exhaust every documented selector, including unused codes, not just defaults.
const voltages=[
  ["dcdc1",0x13,5,c=>c<=19?1500+100*c:null],
  ...[2,3,4].map(n=>["dcdc"+n,0x12+n,7,c=>c<=70?500+10*c:c<=87?20*c-200:null]),
  ["dcdc5",0x17,7,c=>c<=32?800+10*c:c<=68?20*c+480:null],
  ["dcdc6",0x18,5,c=>c<=29?500+100*c:null],
  ["aldo1",0x19,5,c=>c<=26?700+100*c:null],
  ...[2,3,4,5].map(n=>["aldo"+n,0x1e+n,5,c=>c<=26?700+100*c:null]),
  ...[1,2,3,4,5].map(n=>["bldo"+n,0x23+n,5,c=>c<=26?700+100*c:null]),
  ...[1,2,3].map(n=>["cldo"+n,0x28+n,5,c=>c<=26?700+100*c:null]),
  ["cldo4",0x2d,6,c=>c<=35?700+100*c:null]
];
for(const [id,reg,width,expected] of voltages) for(let code=0;code<2**width;code++) {
  const result=field(id+".voltage",reg,code), mv=expected(code);
  assert.equal(result.raw,"0x"+code.toString(16).toUpperCase().padStart(Math.ceil(width/4),"0"));
  assert.equal(result.status,mv===null?"unknown":"known",id+" selector "+code);
  if(mv!==null) assert.equal(result.label,mv+" мВ",id+" selector "+code);
  else assert.match(result.label,/Не описано/);
}
for(let code=0;code<15;code++) assert.equal(field("cpusldo.voltage",0x2e,code).label,(700+50*code)+" мВ");
assert.equal(field("cpusldo.voltage",0x2e,15).label,"VDDR/2 (слежение)");
assert.equal(field("cpusldo.voltage",0x2e,255).label,"VDDR/2 (слежение)");
for(let code=0;code<8;code++) assert.equal(field("power_good.voff",0x1f,code<<3).label,(2600+100*code)+" мВ");
assert.equal(field("dcdc_mode1.ramp",0x1a,0x20).label,"31,250 мкс");

let cache=[entry(0x10,0x02),entry(0x14,70,{updated_ms:1000}),entry(0x15,0),entry(0x1a,0x40),entry(0x1b,2)];
assert.equal(summary(cache).dcdc3_voltage.label,"1200 мВ");
assert.equal(summary(cache).dcdc3_voltage.updated_ms,1000);
assert.equal(summary(cache).dcdc3_enable.label,"Разрешён");
assert.equal(summary(cache).dcdc3_mode.label,"Принудительный PWM");
cache.find(e=>e.reg===0x1a).val=0;
assert.equal(summary(cache).dcdc3_voltage.label,"500 мВ");
assert.equal(summary(cache).dcdc3_enable.label,"Отключён");
assert.equal(summary(cache).dcdc3_mode.label,"Автоматический PFM или PWM");
assert.equal(summary(cache.filter(e=>e.reg!==0x1a)).dcdc3_voltage.status,"missing");
cache=[entry(0x10,0x08),entry(0x16,87),entry(0x18,29),entry(0x1a,0x80),entry(0x1b,8)];
assert.equal(summary(cache).dcdc6_voltage.label,"1540 мВ");
assert.equal(summary(cache).dcdc6_enable.label,"Разрешён");
cache.find(e=>e.reg===0x1a).val=0;
assert.equal(summary(cache).dcdc6_voltage.label,"3400 мВ");
assert.equal(summary(cache).dcdc6_enable.label,"Отключён");
assert.equal(summary([entry(0x12,0x30),entry(0x2b,0),entry(0x2c,0)]).cldo3_enable.label,"Разрешён");
assert.match(summary([entry(0x12,0x30),entry(0x2b,0x40)]).cldo3_enable.label,/GPIO1/);
assert.equal(summary([entry(0x12,0x30),entry(0x2c,3)]).cldo4_enable.label,"GPIO2: вход");
for(const code of [4,5]) assert.equal(field("gpio2.function",0x2c,code).tone,"unknown");
for(const code of [6,7]) assert.equal(field("gpio2.function",0x2c,code).label,"Высокоимпедансное состояние");

for(const ref of ["irq_status1.temperature1","irq_status1.dcdc6","irq_status2.pok_short"]) {
  assert(compiled.fields.get(ref).attributes.includes("write_one_clear"));
  assert(!compiled.fields.get(ref).attributes.includes("read_clear"));
}
assert.equal(field("irq_status1.temperature2",0x48,2).tone,"bad");
assert.equal(field("irq_status2.pok_short",0x49,2).tone,"warn");
assert(compiled.fields.get("power_control.power_off").attributes.includes("self_clear"));
for(const extra of [{valid:false},{source:"settings"},{val:256},{val:-1}]) {
  assert.equal(field("dcdc1.voltage",0x13,0x12,extra).status,"missing");
}
for(const item of decodeRegisterMap(compiled,[]).summary) assert.equal(item.status,"missing");
assert.equal(summary([entry(0x13,0x12,{source:"settings"})]).dcdc1_voltage.status,"missing");

// Full bit coverage and lossless extraction for all 592 bits of the profile.
for(const reg of map.registers) for(let byte=0;byte<256;byte++) {
  assert.equal(reg.fields.reduce((n,f)=>n+(Math.floor(byte/2**f.lsb)%2**f.width)*2**f.lsb,0),byte);
}
assert.throws(()=>validateRegisterMap(map,{bus:"mdio"}));
assert.throws(()=>validateRegisterMap(map,{regCount:13}));
const ranged=m=>m.registers.find(r=>r.id==="dcdc2").fields.find(f=>f.id==="voltage");
for(const change of [
  f=>f.ranges=[], f=>f.type="hex", f=>f.scale=1, f=>f.offset=0,
  f=>f.ranges[1].min=70, f=>f.ranges[0].min=0.5, f=>f.ranges[0].max=128,
  f=>f.ranges[0].scale=null, f=>f.ranges[0].offset=Infinity,
  f=>f.ranges[0].scale=Number.MAX_VALUE, f=>f.ranges[1].max=1,
  f=>f.ranges[0].eval="doSomething()", f=>f.ranges[0].min=-1
]) {
  const invalid=structuredClone(map); change(ranged(invalid));
  assert.throws(()=>validateRegisterMap(invalid));
}
const gap=structuredClone(map); ranged(gap).ranges[1].min=72;
const g=validateRegisterMap(gap);
assert.equal(decodeRegisterMap(g,[entry(0x14,71)]).fields.get("dcdc2.voltage").status,"unknown");
console.log("AXP15060: 74 addresses, 41 documented registers, 190 fields; voltage selectors, paired channels, VDDR/2, GPIO, IRQ, missing/seeded data and range validation passed");
