#!/usr/bin/env node
// Decode CACHE JSON lines using the same declarative engine as the web UI.
// Device view.js files are intentionally not executed: they are browser forms.
import {readFile} from "node:fs/promises";
import {dirname, resolve} from "node:path";
import {createInterface} from "node:readline";
import {pathToFileURL} from "node:url";

export async function createDecoder(profilePath, mapPath) {
  const profile = JSON.parse(await readFile(profilePath, "utf8"));
  if (profile.schema_version !== 2 || !Array.isArray(profile.resources)) throw new Error("Expected a v2 device profile");
  const map = JSON.parse(await readFile(mapPath || resolve(dirname(profilePath), profile.register_map), "utf8"));
  const source = await readFile(new URL("../../web/assets/js/register-map.js", import.meta.url));
  const {validateRegisterMap, decodeRegisterMap} = await import("data:text/javascript;base64," + source.toString("base64"));
  const compiled = validateRegisterMap(map, {bus:profile.transport.bus, regCount:profile.resources.length});
  const resources = new Map(profile.resources.map(r => [r.id, r]));
  if (resources.size !== profile.resources.length) throw new Error("Duplicate resource ID in profile");
  return state => {
    if (state.subsystem !== profile.transport.bus || state.device !== profile.id) throw new Error("CACHE target does not match the local profile");
    if (!state.synchronized) return {device:profile.id, synchronized:false, reason:state.reason, summary:[]};
    if (!Array.isArray(state.resources) || state.resources.length !== resources.size ||
        new Set(state.resources.map(e => e.id)).size !== resources.size) throw new Error("CACHE catalog does not match the local profile");
    const cache = state.resources.map(e => {
      const r = resources.get(e.id);
      const width = r?.width_bits ?? profile.defaults?.[r?.kind]?.width_bits;
      if (!r || r.kind !== "register" || width !== e.width_bits || width !== map.width_bits ||
          typeof e.value !== "string" || !/^0x[0-9a-f]+$/i.test(e.value) || typeof e.valid !== "boolean" ||
          !["unknown","settings","device"].includes(e.source)) throw new Error("CACHE resource does not match the local profile");
      const value = Number(e.value);
      if (!Number.isSafeInteger(value) || value < 0 || value >= 2 ** width) throw new Error("Invalid cached value");
      return {reg:Number(r.address), val:value, valid:e.valid, source:e.source, updated_ms:e.updated_ms};
    });
    const decoded = decodeRegisterMap(compiled, cache);
    return {device:profile.id, synchronized:true, generation:state.generation, revision:state.revision,
      presence:state.presence, identity:decoded.identity,
      summary:decoded.summary.map((item, i) => ({id:item.id, name:map.summary[i].label,
        status:item.status, text:item.label, tone:item.tone, updated_ms:item.updated_ms})),
      fields:Object.fromEntries(decoded.fields)};
  };
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  try {
    if (process.argv.length < 3 || process.argv.length > 4)
      throw new Error("Usage: node scripts/dcp2/cache_indicators.mjs PROFILE.json [REGISTER_MAP.json] < cache.jsonl");
    const decode = await createDecoder(resolve(process.argv[2]), process.argv[3] && resolve(process.argv[3]));
    const input = createInterface({input:process.stdin, crlfDelay:Infinity});
    for await (const line of input) if (line.trim()) console.log(JSON.stringify(decode(JSON.parse(line))));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
