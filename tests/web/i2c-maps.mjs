import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
const source=await readFile(new URL("../../web/assets/js/register-map.js",import.meta.url));
const {validateRegisterMap,decodeRegisterMap}=await import("data:text/javascript;base64,"+source.toString("base64"));
const bundles=["pca9545a","pca9545a/pcf8563","pca9545a/p24c02a","pca9545a/pca9555"];
const entry=(reg,val,extra={})=>({reg,val,valid:true,source:"read",updated_ms:12000,...extra});
for (const bundle of bundles) {
  const text=await readFile(new URL(`../../configs/i2c/${bundle}/register_map.json`,import.meta.url),"utf8");
  assert(Buffer.byteLength(text)<=65536,"map must fit the Flash asset reader");
  assert(!/_u(?:16|17|18|32)|mPCIe|SFP|BKEY|U16|U17|U18|U32/.test(text),"map contains board-specific labels");
  const map=JSON.parse(text), compiled=validateRegisterMap(map,{bus:"i2c"});
  const all=map.registers.map(r=>entry(Number(r.address),255));
  for (const reg of map.registers) for (let word=0;word<256;word++) {
    const recovered=reg.fields.reduce((v,f)=>v+(Math.floor(word/2**f.lsb)%2**f.width)*2**f.lsb,0);
    assert.equal(recovered,word);
  }
  for (const cache of [[],all.map(e=>({...e,valid:false})),all.map(e=>({...e,source:"settings"}))])
    assert(decodeRegisterMap(compiled,cache).summary.every(s=>s.status==="missing"));
  if (map.id==="pca9545a") {
    const d=decodeRegisterMap(compiled,[entry(0,0x81)]);
    assert.equal(d.fields.get("control.ch0").value,1);
    assert.equal(d.fields.get("control.ch1").value,0);
    assert.equal(d.fields.get("control.int3").value,1);
  }
  if (map.id==="p24c02a") {
    const d=decodeRegisterMap(compiled,[entry(255,0xa5)]);
    assert.equal(d.fields.get("bff.v").value,165);
    assert.equal(d.fields.get("b00.v").status,"missing");
  }
  if (map.id==="pca9555") {
    const d=decodeRegisterMap(compiled,[entry(0,1),entry(6,255)]);
    assert.equal(d.fields.get("input0.p0_0").value,1);
    assert.equal(d.fields.get("config0.p0_0").label,"Вход");
    assert.equal(map.summary.length,16);
  }
  console.log(`${map.id}: ${map.register_count} registers, complete bit coverage, unknown/seeded cache rejected`);
}
