import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {fileURLToPath} from "node:url";
import {createDecoder} from "../../scripts/dcp2/cache_indicators.mjs";

for (const [bus, device] of [["mdio","lan8720"],["i2c","axp15060"]]) {
  const file = fileURLToPath(new URL(`../../configs/${bus}/${device}/${device}.json`, import.meta.url));
  const profile = JSON.parse(await readFile(file, "utf8"));
  const decode = await createDecoder(file);
  const state = {subsystem:bus, device, synchronized:true, generation:1, revision:1, presence:"ready",
    resources:profile.resources.map(r => ({id:r.id, width_bits:bus === "mdio" ? 16 : 8,
      value:"0x00", valid:false, source:"unknown", updated_ms:0}))};
  let result = decode(state);
  assert(result.summary.length > 0);
  assert(result.summary.every(e => e.status === "missing"));
  for (const e of state.resources) { e.valid = true; e.source = "device"; e.updated_ms = 1000; }
  if (bus === "mdio") {
    state.resources.find(e => e.id === "reg_02").value = "0x0007";
    state.resources.find(e => e.id === "reg_03").value = "0xC0F1";
    state.resources.find(e => e.id === "reg_01").value = "0x0024";
    state.resources.find(e => e.id === "reg_1f").value = "0x0018";
  }
  result = decode(state);
  assert(result.summary.some(e => e.status === "known"));
  if (bus === "mdio") assert.equal(result.identity, true);
  for (const e of state.resources) e.source = "settings";
  assert(decode(state).summary.every(e => e.status === "missing"));
  assert.deepEqual(decode({...state, synchronized:false}).summary, []);
  assert.throws(() => decode({...state, device:"different"}), /profile/);
}
{
  const profilePath = fileURLToPath(new URL("../host/fixtures/device-multi-space.json",import.meta.url));
  const mapPath = fileURLToPath(new URL("../host/fixtures/device-multi-space_register_map.json",import.meta.url));
  const profile = JSON.parse(await readFile(profilePath,"utf8"));
  const decode = await createDecoder(profilePath,mapPath);
  const state = {subsystem:"i2c",device:"dual",synchronized:true,generation:1,revision:1,presence:"ready",
    resources:profile.resources.map(r => ({id:r.id,width_bits:8,value:r.space==="identity"?"0x11":"0x22",
      valid:true,source:"device",updated_ms:1000}))};
  assert.deepEqual(decode(state).summary.map(s=>s.text),["17","34"]);
  state.resources[4].value="0x33";
  assert.deepEqual(decode(state).summary.map(s=>s.text),["17","51"]);
  state.resources[4].valid=false;
  assert.equal(decode(state).summary[1].status,"missing");
  assert.equal(decode(state).summary[0].text,"17");
}
console.log("DCP2 CACHE indicators tests passed");
