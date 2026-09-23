import * as api from "./api.js";
import {esc,hex,uptime} from "./ui.js";
import {validRegisterMapName,validateRegisterMap,decodeRegisterMap} from "./register-map.js";
import {deviceView} from "./device-view.js";

const stamp = ms => Number.isFinite(ms) ? uptime(ms) + "." + String(ms % 1000).padStart(3,"0") : "—";
const attributes = {latch_low:"Фиксация нуля до чтения",latch_high:"Фиксация единицы до чтения",read_clear:"Чтение очищает",self_clear:"Самосброс",nasr:"Не сбрасывается программным Reset",write_one_clear:"Запись единицы очищает",write_any_clear:"Любая запись очищает"};
const access = {ro:"Только чтение",rw:"Чтение и запись",wo:"Только запись",none:"Доступ не предусмотрен"};
export function bundlePath(bus, fileName, asset) {
  const slash = typeof fileName === "string" ? fileName.lastIndexOf("/") : -1;
  const directory = slash > 0 ? fileName.slice(0,slash) : "";
  const leaf = slash > 0 ? fileName.slice(slash + 1) : fileName;
  const nested = slash > 0 && /^[a-z0-9_-]{1,31}$/i.test(directory) &&
    (leaf.toLowerCase() === "profile.json" || leaf.toLowerCase() === directory.toLowerCase() + ".json");
  const flat = slash < 0 && typeof fileName === "string" && /^[a-z0-9_.-]+\.json$/i.test(fileName);
  if (!["i2c","mdio"].includes(bus) || typeof fileName !== "string" || fileName.length >= 48 ||
      (!nested && !flat) || fileName.includes("..") ||
      typeof asset !== "string" || asset.length >= 64 || !/^[a-zA-Z0-9_-]+\.(json|js)$/.test(asset)) throw Error("Неверный путь пакета устройства");
  return `/config/${bus}/${slash < 0 ? "" : fileName.slice(0,slash+1)}${asset}`;
}
export function indicatorsWorkspace(root, {bus,detail,signal,busy = () => false}) {
  const controller = new AbortController(), cancel = () => controller.abort();
  signal.addEventListener("abort",cancel,{once:true});
  if (signal.aborted) cancel();
  let compiled, custom, timer = 0, stopped = false, stale = false;
  controller.signal.addEventListener("abort",() => { clearTimeout(timer); custom?.dispose(); },{once:true});
  const active = () => !stopped && !controller.signal.aborted;
  const message = value => {
    const target = root.querySelector("[data-map-message]");
    if (target) target.textContent = value;
  };
  root.innerHTML = '<article class="card"><h2>Показатели</h2><p data-map-message>Загрузка карты регистров…</p></article>';

  function paint() {
    if (!compiled || !active()) return;
    const {map} = compiled, result = decodeRegisterMap(compiled,detail.cache || []);
    const identity = root.querySelector("[data-map-identity]");
    identity.textContent = !map.identity ? "Карта выбрана по профилю устройства; проверка идентификатора не задана." : result.identity === true ? "Идентификатор соответствует карте" : result.identity === false ? "Идентификатор не соответствует карте" : "Идентификатор не проверен по кэшу";
    identity.className = result.identity === false ? "error-text" : "muted";
    root.querySelector("[data-map-fresh]").textContent = stale ? "Связь потеряна. Показан последний полученный снимок." : detail.presence === "responding" ? "Данные последнего кэширования. Метки времени отсчитываются от старта системы." : "Устройство не готово. Доступные значения относятся к последним взаимодействиям.";
    for (const item of result.summary) {
      const node = root.querySelector(`[data-indicator="${item.id}"]`);
      node.querySelector("strong").textContent = item.label;
      node.querySelector("strong").className = "indicator-value " + item.tone;
      node.querySelector("time").textContent = stamp(item.updated_ms);
    }
    const cache = new Map((detail.cache || []).map(e => [e.reg,e]));
    for (const reg of map.registers) {
      const entry = cache.get(Number(reg.address)), node = root.querySelector(`[data-decoded-register="${reg.id}"]`);
      node.querySelector("[data-raw]").textContent = entry?.valid ? hex(entry.val,map.width_bits/4) + (entry.source === "settings" ? " (настройки, не чтение)" : "") : "Нет данных";
      node.querySelector("time").textContent = stamp(entry?.valid && entry.source !== "settings" ? entry.updated_ms : null);
      for (const field of reg.fields) {
        const target = node.querySelector(`[data-field="${field.id}"]`), decoded = result.fields.get(reg.id+"."+field.id);
        target.querySelector("[data-value]").textContent = decoded.label;
        target.querySelector("[data-value]").className = "indicator-value " + decoded.tone;
        target.querySelector("[data-field-raw]").textContent = decoded.raw ?? "—";
      }
    }
    custom?.update({api_version:1,device:{id:detail.name || detail.config.name,bus,presence:detail.presence || "unknown"},stale,
      identity:result.identity,map,cache:detail.cache || [],
      summary:result.summary.map((item,i) => ({...item,title:map.summary[i].label})),fields:Object.fromEntries(result.fields)});
  }
  function render() {
    const {map} = compiled;
    root.innerHTML = `<article class="card indicators-card"><h2>Показатели ${esc(map.device)}</h2><p data-map-fresh class="muted"></p><p data-map-identity></p><p data-map-message class="error-text"></p><div data-device-view></div><div class="indicator-grid" data-standard-summary>${map.summary.map(item => `<section data-indicator="${item.id}" class="indicator-tile"><span>${esc(item.label)}</span><strong class="indicator-value">Нет данных</strong><small>Метка времени <time>—</time></small>${item.note ? `<p class="muted">${esc(item.note)}</p>` : ""}</section>`).join("")}</div><p class="muted indicator-note">${esc(map.notes)}</p><p class="muted">Карта по <a href="${esc(map.source.url)}" target="_blank" rel="noopener noreferrer">${esc(map.source.document)} ${esc(map.source.revision)}</a>. Просмотр не выполняет физических чтений или записей.</p></article><article class="card"><h2>Расшифровка регистров</h2><div class="decoded-registers">${map.registers.map(reg => `<details data-decoded-register="${reg.id}"><summary><span class="mono">${hex(Number(reg.address))}</span> ${esc(reg.label)} <span class="mono" data-raw></span></summary><div class="decoded-body"><p class="muted">${esc(reg.name)}. Документация: ${esc(reg.section)}. Последнее обновление <time>—</time></p>${reg.note ? `<p>${esc(reg.note)}</p>` : ""}<div class="table-scroll"><table><thead><tr><th>Биты</th><th>Поле</th><th>Код</th><th>Расшифровка</th></tr></thead><tbody>${reg.fields.map(field => `<tr data-field="${field.id}"><td class="mono">${field.width > 1 ? field.lsb+field.width-1+":" : ""}${field.lsb}</td><td>${esc(field.label)}<small>${access[field.access]}. После сброса: ${esc(field.reset)}${field.attributes?.length ? ". "+field.attributes.map(a => attributes[a]).join(". ") : ""}${field.write_value !== undefined ? ". При записи: "+hex(Number(field.write_value)) : ""}</small></td><td class="mono" data-field-raw></td><td><span data-value></span>${field.note ? `<small>${esc(field.note)}</small>` : ""}</td></tr>`).join("")}</tbody></table></div></div></details>`).join("")}</div></article>`;
    paint();
  }
  async function readAsset(name) {
    const value = await api.request(api.fileUrl("flash",bundlePath(bus,detail.file_name,name),65536),{signal:controller.signal,raw:true});
    if (new TextEncoder().encode(value).length > 65536) throw Error("Файл пакета больше 64 КиБ");
    return value;
  }
  async function poll() {
    if (!active()) return;
    if (!document.hidden && !busy() && !api.pending && Date.now() >= api.downloadPauseUntil) {
      try {
        const next = await api.busCache(bus,detail.config.name,controller.signal);
        if (active() && Array.isArray(next.cache)) { detail.cache = next.cache; stale = false; paint(); }
      } catch (error) { if (active()) { stale = true; paint(); } }
    }
    if (active()) timer = setTimeout(poll,500);
  }
  async function start() {
    if (!detail.register_map) { message("Карта регистров не указана в профиле устройства."); return; }
    try {
      if (!validRegisterMapName(detail.register_map)) throw Error("Неверное имя карты регистров");
      const map = JSON.parse(await readAsset(detail.register_map));
      if (!active()) return;
      compiled = validateRegisterMap(map,{bus,regCount:detail.config.reg_count}); render();
      timer = setTimeout(poll,500);
      if (detail.view) {
        try {
          const source = await readAsset(detail.view);
          if (!active()) return;
          custom = deviceView(root.querySelector("[data-device-view]"),source,{
            onReady:() => { root.querySelector("[data-standard-summary]").hidden = true; },
            onError:error => { root.querySelector("[data-standard-summary]").hidden = false; message(error); }
          });
          paint();
        } catch (error) { if (active()) message("Форма устройства недоступна. Показана стандартная расшифровка. " + error.message); }
      }
    } catch (error) { if (active()) message("Не удалось загрузить карту. " + error.message); }
  }
  void start();
  return {
    update(next) { detail = next; stale = false; paint(); },
    dispose() { stopped = true; clearTimeout(timer); custom?.dispose(); cancel(); signal.removeEventListener("abort",cancel); }
  };
}
