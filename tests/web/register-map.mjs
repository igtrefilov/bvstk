import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
const source = await readFile(new URL("../../web/assets/js/register-map.js",import.meta.url));
const {validateRegisterMap,decodeRegisterMap,validRegisterMapName} = await import("data:text/javascript;base64,"+source.toString("base64"));
const map = JSON.parse(await readFile(new URL("../../configs/mdio/lan8720/register_map.json",import.meta.url),"utf8"));
const compiled = validateRegisterMap(map,{bus:"mdio",regCount:32});
assert.equal(map.registers.length,32);
assert.deepEqual(map.registers.filter(r=>!r.reserved).map(r=>Number(r.address)),[0,1,2,3,4,5,6,17,18,26,27,29,30,31]);
assert.equal(compiled.fields.size,119);
assert.equal(validRegisterMapName("register_map.json"),true);
assert.equal(validRegisterMapName("lan8720_register_map.json"),true);
for (const bad of ["../register_map.json","x/register_map.json","profile.json",null]) assert.equal(validRegisterMapName(bad),false);
const entry = (reg,val,extra={}) => ({reg,val,valid:true,source:"read",updated_ms:123456,...extra});
const cache = () => [entry(0,0x1000),entry(1,0x782d),entry(2,7),entry(3,0xc0f1),entry(17,2),entry(18,0xe1),entry(26,15),entry(27,0),entry(31,0x1018)];
const summary = (values=cache()) => Object.fromEntries(decodeRegisterMap(compiled,values).summary.map(item=>[item.id,item]));
assert.equal(decodeRegisterMap(compiled,cache()).identity,true);
for (const [code,speed,duplex] of [[1,"10 Мбит/с","Полудуплекс"],[5,"10 Мбит/с","Полный дуплекс"],[2,"100 Мбит/с","Полудуплекс"],[6,"100 Мбит/с","Полный дуплекс"]]) {
  const c=cache(); c.find(e=>e.reg===31).val=0x1000|(code<<2);
  assert.equal(summary(c).speed.label,speed); assert.equal(summary(c).duplex.label,duplex);
}
for (const code of [0,3,4,7]) {
  const c=cache(); c.find(e=>e.reg===31).val=0x1000|(code<<2);
  assert.equal(summary(c).speed.status,"unknown");
}
let c=cache(); c.find(e=>e.reg===0).val=0x2100;
assert.equal(summary(c).speed.label,"100 Мбит/с"); assert.equal(summary(c).duplex.label,"Полный дуплекс");
c=cache(); c.find(e=>e.reg===1).val &= ~4;
assert.match(summary(c).link.label,/обрыв/); assert.equal(summary(c).speed.label,"Соединение не подтверждено");
c=cache(); c.find(e=>e.reg===1).val &= ~32;
assert.equal(summary(c).speed.label,"Автосогласование не завершено");
assert.equal(summary(cache().filter(e=>e.reg!==31)).speed.status,"missing");
assert.equal(summary(cache().filter(e=>e.reg!==0)).speed.status,"missing");
assert.equal(summary(cache().map(e=>({...e,source:"settings"}))).speed.status,"missing");
assert.equal(summary(cache().map(e=>({...e,valid:false}))).power.status,"missing");
assert.equal(summary([]).link.status,"missing");
c=cache(); c.find(e=>e.reg===2).val=0xffff;
assert.equal(decodeRegisterMap(compiled,c).identity,false); assert.equal(summary(c).link.status,"missing");
c=cache(); c.find(e=>e.reg===31).updated_ms=1000;
assert.equal(summary(c).speed.updated_ms,1000);
const all=decodeRegisterMap(compiled,Array.from({length:32},(_,r)=>entry(r,65535)));
for (const [ref,f] of compiled.fields) assert.equal(all.fields.get(ref).value,2**f.width-1);
assert.equal(all.fields.get("secr.errors").label,"65535");
assert.equal(all.fields.get("bsr.remote_fault").label,"Зафиксирована");
assert.equal(all.fields.get("bsr.link").tone,"ok");
// Width/mask coverage is validated; every possible word can be reconstructed.
for (const reg of map.registers) for (let word=0;word<=65535;word++) {
  const recovered=reg.fields.reduce((v,f)=>v+(Math.floor(word/2**f.lsb)%2**f.width)*2**f.lsb,0);
  assert.equal(recovered,word);
}
for (const change of [m=>delete m.id,m=>delete m.source.title,m=>m.width_bits=8,m=>m.registers[1].address="0x00",
  m=>m.registers[0].fields[0].lsb=14,m=>m.registers[0].fields.pop(),m=>m.summary[0].field="no.such_field",
  m=>m.summary[1].cases[0].when[0].equals=3,m=>m.summary[0].eval="alert(1)",m=>m.registers[0].fields[0].type="eval"]) {
  const invalid=structuredClone(map); change(invalid); assert.throws(()=>validateRegisterMap(invalid));
}
assert.throws(()=>validateRegisterMap(map,{bus:"i2c"}));
assert.throws(()=>validateRegisterMap(map,{regCount:16}));
console.log("Register map: 32 addresses, 119 bit fields, 4 link modes, missing/seeded/latched data, schema validation passed");
