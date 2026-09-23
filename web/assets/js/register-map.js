// Declarative decoding only. This module has no DOM, network or bus access.
const identifier = /^[a-z][a-z0-9_]*$/;
const validId = value => typeof value === "string" && value.length <= 64 && identifier.test(value);
const tones = new Set(["ok", "warn", "bad", "offline", "unknown"]);
const own = (o, key) => Object.prototype.hasOwnProperty.call(o, key);
const fail = message => { throw new Error("Карта регистров: " + message); };
function object(o) { return o !== null && typeof o === "object" && !Array.isArray(o); }
function keys(o, allowed) {
  if (!object(o) || Object.keys(o).some(k => !allowed.includes(k))) fail("неизвестные поля или неверный тип объекта");
}
function text(value, max = 600) { if (typeof value !== "string" || !value.length || value.length > max) fail("некорректная строка"); }
function uint(value, max = 65535) {
  if (typeof value === "string" && /^0x[0-9a-f]+$/i.test(value)) value = Number(value);
  if (!Number.isInteger(value) || value < 0 || value > max) fail("число вне допустимого диапазона");
  return value;
}
function list(value, max, min = 0) { if (!Array.isArray(value) || value.length < min || value.length > max) fail("неверный размер списка"); }
function label(value) {
  keys(value,["label","tone"]); text(value.label);
  if (value.tone !== undefined && !tones.has(value.tone)) fail("неизвестный цвет состояния");
}
function values(entries, max) {
  if (!object(entries) || Object.keys(entries).length > 256) fail("неверная таблица значений");
  for (const [key, value] of Object.entries(entries)) {
    if (!/^(0|[1-9][0-9]*)$/.test(key)) fail("код перечисления должен быть десятичным");
    uint(Number(key), max); label(value);
  }
}
export function validRegisterMapName(name) {
  return typeof name === "string" && name.length < 64 && /^[a-zA-Z0-9_-]+\.json$/.test(name) &&
    (name === "register_map.json" || /_register_map\.json$/i.test(name));
}
export function validateRegisterMap(map, {bus, regCount} = {}) {
  keys(map,["schema_version","kind","id","device","bus","width_bits","register_count","source","notes","identity","registers","summary"]);
  if (map.schema_version !== 1 || map.kind !== "register_map") fail("неподдерживаемая версия");
  if (!validId(map.id)) fail("неверный идентификатор");
  text(map.device); text(map.notes);
  if (!["mdio","i2c"].includes(map.bus) || (bus && map.bus !== bus)) fail("шина не совпадает с профилем");
  if (map.width_bits !== (map.bus === "mdio" ? 16 : 8)) fail("неверная разрядность");
  const count = uint(map.register_count, map.bus === "mdio" ? 32 : 256);
  if (!count || (regCount !== undefined && count !== regCount)) fail("число регистров не совпадает с профилем");
  keys(map.source,["title","document","revision","url"]);
  for (const key of ["title","document","revision","url"]) text(map.source[key]);
  if (!/^https:\/\//.test(map.source.url)) fail("неверная ссылка на документацию");
  list(map.registers,count,count);
  const fields = new Map(), addresses = new Set(), ids = new Set();
  for (const reg of map.registers) {
    keys(reg,["id","address","name","label","section","reserved","note","fields"]);
    if (!validId(reg.id) || ids.has(reg.id)) fail("повторяющийся или неверный ID регистра");
    ids.add(reg.id);
    const addr = uint(reg.address,count-1);
    if (addresses.has(addr)) fail("повторяющийся адрес");
    addresses.add(addr);
    text(reg.name); text(reg.label); text(reg.section);
    if (reg.reserved !== undefined && typeof reg.reserved !== "boolean") fail("reserved должен быть boolean");
    if (reg.note !== undefined) text(reg.note);
    list(reg.fields,32,1);
    let used = 0;
    for (const field of reg.fields) {
      keys(field,["id","label","lsb","width","type","access","reset","attributes","values","note","unit","scale","offset","ranges","write_value"]);
      const ref = reg.id + "." + field.id;
      if (!validId(field.id) || fields.has(ref)) fail("повторяющееся или неверное поле");
      text(field.label); text(field.reset);
      const lsb = uint(field.lsb,map.width_bits-1), width = uint(field.width,map.width_bits);
      if (!width || lsb + width > map.width_bits) fail("поле выходит за границы регистра");
      const max = 2 ** width - 1, mask = max * 2 ** lsb;
      if (used & mask) fail("перекрывающиеся битовые поля");
      used |= mask;
      if (!["enum","uint","hex","reserved"].includes(field.type) || !["ro","rw","wo","none"].includes(field.access)) fail("неизвестный тип поля или доступа");
      if (reg.reserved && (field.type !== "reserved" || field.access !== "none")) fail("зарезервированный адрес не должен описывать рабочие поля");
      if (field.type === "enum") values(field.values,max);
      else if (field.values !== undefined) fail("таблица значений допустима только для enum");
      if (field.attributes !== undefined) {
        list(field.attributes,8);
        if (field.attributes.some(a => !["latch_low","latch_high","read_clear","self_clear","nasr","write_one_clear","write_any_clear"].includes(a))) fail("неизвестный атрибут поля");
      }
      for (const key of ["scale","offset"]) if (field[key] !== undefined && (field.type !== "uint" || !Number.isFinite(field[key]))) fail("неверный коэффициент пересчёта");
      if (field.ranges !== undefined) {
        if (field.type !== "uint" || field.scale !== undefined || field.offset !== undefined) fail("диапазоны допустимы только для uint без общих коэффициентов");
        list(field.ranges,32,1);
        let previous = -1;
        for (const range of field.ranges) {
          keys(range,["min","max","scale","offset"]);
          const low = uint(range.min,max), high = uint(range.max,max);
          if (low > high || low <= previous || !Number.isFinite(range.scale) || !Number.isFinite(range.offset) ||
              !Number.isFinite(low * range.scale + range.offset) || !Number.isFinite(high * range.scale + range.offset)) fail("неверные или пересекающиеся диапазоны");
          previous = high;
        }
      }
      if (field.unit !== undefined) text(field.unit,40);
      if (field.note !== undefined) text(field.note);
      if (field.write_value !== undefined) uint(field.write_value,max);
      fields.set(ref,{...field,reg:addr,register:reg.id,max});
    }
    if (used !== 2 ** map.width_bits - 1) fail("описаны не все биты регистра");
  }
  const condition = cond => {
    keys(cond,["field","equals"]);
    const field = fields.get(cond.field);
    if (!field || field.type === "reserved") fail("неизвестное поле в условии");
    uint(cond.equals,field.max);
  };
  if (map.identity !== undefined) { list(map.identity,16,1); map.identity.forEach(condition); }
  list(map.summary,64,1);
  const summaryIds = new Set();
  for (const item of map.summary) {
    keys(item,["id","label","field","values","cases","note"]);
    if (!validId(item.id) || summaryIds.has(item.id)) fail("неверный ID показателя");
    summaryIds.add(item.id); text(item.label);
    if (item.note !== undefined) text(item.note);
    const output = entry => {
      if (entry.field !== undefined) {
        const field = fields.get(entry.field);
        if (!field || field.type === "reserved" || entry.label !== undefined || entry.tone !== undefined) fail("неверный источник показателя");
        if (entry.values !== undefined) values(entry.values,field.max);
      } else {
        if (entry.values !== undefined) fail("таблица без источника");
        label({label:entry.label,...(entry.tone !== undefined ? {tone:entry.tone} : {})});
      }
    };
    if (item.field !== undefined) {
      if (item.cases !== undefined) fail("неоднозначный показатель");
      output({field:item.field,...(item.values !== undefined ? {values:item.values} : {})});
    } else {
      if (item.values !== undefined) fail("таблица без источника");
      list(item.cases,32,1);
      item.cases.forEach(entry => { keys(entry,["when","field","values","label","tone"]); list(entry.when,16,1); entry.when.forEach(condition); output(entry); });
    }
  }
  return {map,fields};
}
const rawHex = (v, bits) => "0x" + v.toString(16).toUpperCase().padStart(Math.ceil(bits/4),"0");
const missing = label => ({status:"missing",label:label || "Нет данных",tone:"unknown",raw:null,updated_ms:null});
function formatted(field, value, overrides) {
  const raw = rawHex(value,field.width), entries = overrides || field.values;
  if (entries) return own(entries,String(value)) ? {status:"known",tone:"offline",...entries[String(value)],raw} : {status:"unknown",label:`Не описано (${raw})`,tone:"unknown",raw};
  if (field.type === "reserved") return {status:"reserved",label:`Зарезервировано (${raw})`,tone:"unknown",raw};
  const range = field.ranges?.find(r => value >= Number(r.min) && value <= Number(r.max));
  if (field.ranges && !range) return {status:"unknown",label:`Не описано (${raw})`,tone:"unknown",raw};
  const number = value * (range?.scale ?? field.scale ?? 1) + (range?.offset ?? field.offset ?? 0);
  return {status:"known",label:field.type === "hex" ? raw : String(Number(number.toPrecision(12))) + (field.unit ? " " + field.unit : ""),tone:"offline",raw};
}
export function decodeRegisterMap(compiled, cache = []) {
  const {map,fields} = compiled, entries = new Map(cache.map(entry => [entry.reg,entry]));
  const decoded = new Map();
  for (const [ref,field] of fields) {
    const entry = entries.get(field.reg);
    let result;
    if (entry?.valid !== true || field.access === "wo" || !Number.isInteger(entry.val) || entry.val < 0 || entry.val >= 2 ** map.width_bits) result = missing();
    else if (entry.source === "settings") result = missing("Нет данных от устройства");
    else {
      const value = Math.floor(entry.val / 2 ** field.lsb) % 2 ** field.width;
      result = {...formatted(field,value),value,updated_ms:entry.updated_ms};
    }
    decoded.set(ref,result);
  }
  // Unknown dependencies stay unknown; they never become a false condition.
  const matches = conditions => {
    let unknown = false;
    for (const condition of conditions) {
      const result = decoded.get(condition.field);
      if (result.status === "missing") unknown = true;
      else if (result.value !== uint(condition.equals)) return false;
    }
    return unknown ? null : true;
  };
  const identity = map.identity ? matches(map.identity) : null;
  const summary = map.summary.map(item => {
    if (identity === false) return {...item,...missing("Идентификатор устройства не совпадает")};
    let output = item, dependencies = [];
    if (item.cases) {
      output = null;
      for (const rule of item.cases) {
        dependencies.push(...rule.when.map(c => c.field));
        const match = matches(rule.when);
        if (match === null) return {...item,...missing()};
        if (match) { output = rule; break; }
      }
      if (!output) return {...item,status:"unknown",label:"Не определено",tone:"unknown",updated_ms:null};
    }
    let result;
    if (output.field) {
      const field = fields.get(output.field), value = decoded.get(output.field);
      dependencies.push(output.field);
      result = value.status === "missing" ? value : {...formatted(field,value.value,output.values),updated_ms:value.updated_ms};
    } else result = {status:"known",label:output.label,tone:output.tone || "offline"};
    const times = dependencies.map(ref => decoded.get(ref).updated_ms).filter(Number.isFinite);
    return {...item,...result,updated_ms:times.length ? Math.min(...times) : null};
  });
  return {identity,summary,fields:decoded};
}
