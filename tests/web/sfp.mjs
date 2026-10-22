import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import vm from "node:vm";
const dir = new URL("../../configs/i2c/pca9545a/sfp/", import.meta.url);
const profile = JSON.parse(await readFile(new URL("sfp.json", dir), "utf8"));
const mapText = await readFile(new URL("register_map.json", dir), "utf8");
const view = await readFile(new URL("view.js", dir), "utf8");
for (const text of [mapText, view]) assert(Buffer.byteLength(text) <= 65536);
const source = await readFile(new URL("../../web/assets/js/register-map.js", import.meta.url));
const {validateRegisterMap, decodeRegisterMap} = await import("data:text/javascript;base64," + source.toString("base64"));
let first = 0;
const spaces = profile.transport.address_spaces.map(s => {
  const result = {...s, address:Number(s.address), first}; first += s.register_count; return result;
});
const map = JSON.parse(mapText);
const compiled = validateRegisterMap(map, {bus:"i2c", regCount:256, addressSpaces:spaces});
assert.equal(profile.transport.parent, "pca9545a");
assert.equal(profile.transport.channel, 1);
assert.equal(profile.defaults.register.access, "ro");
assert.equal(profile.resources.find(r => r.id === "a2_6e").access, "rw");
assert.equal(map.registers.find(r => r.id === "a2_6e").fields.find(f => f.id === "soft_disable").access, "rw");
assert.equal(map.registers.find(r => r.id === "a2_6e").fields.find(f => f.id === "soft_rs0").access, "ro");
assert.equal(map.registers.find(r => r.id === "a2_72").fields.every(f => f.access === "ro"), true);
assert.equal(map.registers.find(r => r.id === "a2_73").fields.every(f => f.access === "ro"), true);
assert.equal(map.registers.find(r => r.id === "a2_76").fields[0].access, "ro");
assert.equal(map.registers.find(r => r.id === "a2_7f").fields[0].access, "ro");
assert.deepEqual(profile.policy.write.rules, []);
assert.deepEqual(profile.settings.cache_seed, []);
assert.equal(profile.settings.apply_on_startup, false);
assert.equal(profile.resources.length, 256);
assert.equal(new Set(profile.resources.map(r => r.id)).size, 256);
for (const [i, r] of profile.resources.entries()) {
  assert.equal(r.space, i < 128 ? "identity" : "diagnostics");
  assert.equal(Number(r.address), i % 128);
  const mapped = map.registers.find(m => m.space === r.space && Number(m.address) === Number(r.address));
  assert(mapped);
  for (let raw = 0; raw <= 255; raw++)
    assert.equal(mapped.fields.reduce((v, f) => v + (raw >> f.lsb & (2 ** f.width - 1)) * 2 ** f.lsb, 0), raw);
}
const sandbox = {window:{}};
vm.runInNewContext(view, sandbox);
const {decode} = sandbox.window.bvstkView;
const cache = new Map();
function put(reg, val, extra = {}) { cache.set(reg, {reg, val, valid:true, source:"I2C", updated_ms:12000, ...extra}); }
function word(reg, n) { n &= 65535; put(128 + reg, n >> 8); put(129 + reg, n & 255); }
function float(reg, n) {
  const b = Buffer.alloc(4); b.writeFloatBE(n);
  [...b].forEach((v, i) => put(128 + reg + i, v));
}
function snapshot(extra = {}) {
  return {device:{id:"sfp", bus:"i2c", presence:"responding", address_spaces:spaces},
    stale:false, identity:true, cache:[...cache.values()], ...extra};
}
assert.match(decode(snapshot()).message, /не прочитан/);
put(0, 3);
assert.equal(decode(snapshot()).rows.temperature.value, null);
put(92, 0x68); // Internal DDM, average RX power.
put(93, 0xF0); // Optional flags and monitoring.
put(65, 2); // RX_LOS has normal polarity.
put(128 + 110, 0);
put(12, 103);
word(96, 25 * 256); word(98, 33000); word(100, 5000); word(102, 10000); word(104, 5000);
for (let i = 0; i < 16; i++) put(20 + i, "SFP TEST".padEnd(16, " ").charCodeAt(i));
let d = decode(snapshot());
assert.equal(d.rows.temperature.value, 25);
assert(Math.abs(d.rows.voltage.value - 3.3) < 1e-12);
assert.equal(d.rows.bias.value, 10);
assert.equal(d.rows.tx_power.value, 1);
assert.equal(d.rows.rx_power.value, 0.5);
assert.match(d.rows.rx_power.text, /-3\.01 dBm/);
assert.equal(d.rows.vendor.text, "SFP TEST");
assert.equal(d.rows.rate.text, "10300 МБод");
assert.equal(decode(snapshot({uptime_ms:18000})).rows.temperature.stale, true);
assert.equal(decode(snapshot({uptime_ms:12001})).rows.temperature.stale, false);
put(128 + 97, 0, {updated_ms:12001});
assert.equal(decode(snapshot()).rows.temperature.value, null, "even a small skew is not a coherent pair");
word(96, 25 * 256);
word(96, -10 * 256);
assert.equal(decode(snapshot()).rows.temperature.value, -10);
put(128 + 97, 0, {updated_ms:14001});
assert.equal(decode(snapshot()).rows.temperature.value, null, "torn pair must stay unknown");
word(96, 25 * 256);
put(128 + 97, 0, {valid:false});
assert.equal(decode(snapshot()).rows.temperature.value, null);
word(96, 25 * 256);
put(128 + 97, 0, {source:"settings"});
assert.equal(decode(snapshot()).rows.temperature.value, null, "seeded data is not a measurement");
word(96, 25 * 256);
put(128 + 110, 1);
assert.equal(decode(snapshot()).rows.voltage.value, null, "Data_Not_Ready gates all measured values");
put(128 + 110, 0x40);
assert.equal(decode(snapshot()).rows.tx_power.value, null, "disabled transmitter power is invalid");
put(128 + 110, 6);
d = decode(snapshot());
assert.equal(d.rows.fault.text, "Есть");
assert.equal(d.rows.fault.tone, "bad");
assert.equal(d.rows.los.text, "Потеря сигнала");
put(65, 4);
assert.equal(decode(snapshot()).rows.los.text, "Сигнал обнаружен", "inverted LOS must not be reported as loss");
put(65, 6);
assert.equal(decode(snapshot()).rows.los.text, "Полярность не определена");
put(65, 2);
put(128 + 110, 0);
put(128 + 112, 0x80); put(128 + 113, 0);
assert.match(decode(snapshot()).rows.alarms.text, /Температура выше/);
assert.equal(decode(snapshot()).rows.alarms.tone, "bad");
put(128 + 116, 0); put(128 + 117, 0);
assert.equal(decode(snapshot()).rows.warnings.text, "Нет установленных");
put(93, 0);
assert.equal(decode(snapshot()).rows.los.text, "Не поддерживается");
put(92, 0);
assert.equal(decode(snapshot()).rows.voltage.value, null);
put(92, 0x6C); // Legacy address switching is not performed by this view.
assert.match(decode(snapshot()).rows.temperature.text, /переключение адреса/);
put(92, 0x78); // Both calibration methods claimed: ambiguous.
assert.equal(decode(snapshot()).rows.voltage.value, null);
put(92, 0x58); // External calibration; no default slope/offset is invented.
assert.equal(decode(snapshot()).rows.temperature.value, null);
for (const reg of [76, 80, 84, 88]) { word(reg, 256); word(reg + 2, 0); }
for (const reg of [56, 60, 64, 68, 72]) float(reg, reg === 68 ? 1 : 0);
d = decode(snapshot());
assert.equal(d.rows.temperature.value, 25);
assert.equal(d.rows.rx_power.value, 0.5);
word(84, 512); word(86, -256);
assert.equal(decode(snapshot()).rows.temperature.value, 49);
float(68, NaN);
assert.equal(decode(snapshot()).rows.rx_power.value, null);
put(12, 255); put(66, 112);
assert.equal(decode(snapshot()).rows.rate.text, "28000 МБод");
put(0, 0xFF);
assert.equal(Object.keys(decode(snapshot()).rows).length, 0);
put(0, 3);
assert.match(decode(snapshot({stale:true})).message, /Связь.*потеряна/);
for (const entries of [[], [...cache.values()].map(e => ({...e, valid:false})),
  [...cache.values()].map(e => ({...e, source:"settings"}))]) {
  assert(decodeRegisterMap(compiled, entries).summary.every(s => s.status === "missing"));
}
// Full identity checksums use only complete byte ranges.
cache.clear();
for (let reg = 0; reg < 96; reg++) put(reg, 0);
put(0, 3); put(63, 3);
assert.equal(decode(snapshot()).rows.base_crc.text, "Совпадает");
put(63, 4);
assert.equal(decode(snapshot()).rows.base_crc.text, "Не совпадает");
assert.equal(decode(snapshot()).rows.encoding.text, "Ошибка контрольной суммы");
assert.equal(decode(snapshot()).rows.rate.text, "Ошибка контрольной суммы");
assert.match(decode(snapshot()).message, /Контрольная сумма/);
put(63, 3);put(92,0x68);put(93,0xF0);put(95,(0x68+0xF0)&255);
put(128+110,0);word(96,25*256);
for(let i=0;i<96;i++) put(128+i,0);
word(0,85*256);word(2,-10*256);word(4,80*256);word(6,-5*256);
put(128+95,1); // Deliberately corrupt threshold/calibration checksum.
assert.equal(decode(snapshot()).rows.temperature_limit0.value,null);
assert.equal(decode(snapshot()).rows.temperature.value,25,"internal measurement does not depend on DMI coefficients");
put(92,0x58);put(95,(0x58+0xF0)&255);
assert.equal(decode(snapshot()).rows.temperature.value,null,"external coefficients require correct DMI checksum");
put(95,0);
assert.equal(decode(snapshot()).rows.temperature.value,null,"bad options checksum must gate DDM");
console.log("SFP: 256 resources, two windows, full map coverage, DDM calibration, flags, missing/seeded/torn data and checksums passed");
