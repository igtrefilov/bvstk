import * as api from "./api.js";
import {esc, hex, head, kv, badge, footer, ask, notice, number, clearNotice, deviceState} from "./ui.js";

import {registerWorkspace} from "./registers.js";
import {policyWorkspace} from "./policy.js";
import {observationWorkspace} from "./observe.js";

const clone = value => JSON.parse(JSON.stringify(value));
export async function busPage(root, params, ctx, bus) {
  const mdio = bus === "mdio", label = mdio ? "MDIO" : "I²C";
  let devices, selected = params.get("name"), tab = params.get("tab") || "overview";
  let detail, draft, invalid = false, busy = 0, fresh = "", currentDevice;
  let ruleIds, regState = {}, ruleState = {list:"whitelist",index:0};
  const reads = new Map();
  const observations = new Map();
  let stopObservation = () => {};
  function resetDraft() {
    draft = clone(detail.config); invalid = false;
    ruleIds = Object.fromEntries(["whitelist","blacklist"].map(key => [key,draft[key].map((_,i) => i)]));
  }
  const tabs = {overview:"Состояние",registers:"Регистры",policy:"Политика",telemetry:"Телеметрия",events:"События"};
  if (tab === "cache") tab = "registers";
  if (!tabs[tab]) tab = "overview";
  const changed = () => !!detail && (invalid || JSON.stringify(draft) !== JSON.stringify(detail.config));
  const maxValue = () => mdio ? 65535 : draft.max_value_code;
  const maxReg = () => draft.reg_count - 1;
  async function locked(work) {
    busy++; root.inert = true;
    try { return await work(); }
    finally { busy--; root.inert = !!busy; updateBar(); }
  }
  async function load(force = false) {
    if (!force && changed() && !await ask({title:"Отбросить черновик?",body:"<p>Изменения в браузере будут потеряны. Уже применённая конфигурация RAM не откатывается.</p>"})) return;
    await locked(async () => {
    devices = await api.busList(bus, ctx.signal);
    if (!devices.devices.length) {
      root.innerHTML = `${head(label,"Подсистема") }<article class="card"><h2>${devices.ready ? "Нет настроенных устройств" : "Сервис недоступен"}</h2><p>Проверьте конфигурацию и диагностический вывод системы.</p></article>`;
      return;
    }
    if (!devices.devices.some(d => d.name === selected)) selected = devices.devices[0].name;
    const next = await api.busGet(bus, selected, ctx.signal);
    if (ctx.signal.aborted) return;
    if (currentDevice !== selected) { reads.clear(); regState = {}; ruleState = {list:"whitelist",index:0}; currentDevice = selected; }
    detail = next; resetDraft(); fresh = footer(); root.classList.remove("is-stale"); render();
    });
  }
  function saveBar() {
    const dirty = changed();
    let text = invalid ? "Исправьте значения полей" : dirty ? "Черновик в браузере" : detail.saved === true ? "Конфигурация сохранена" : detail.saved === false ? "Применено в RAM, не сохранено во Flash" : "Состояние сохранения неизвестно";
    return `<div class="save-state"><span class="dot ${dirty || detail.saved !== true ? "warn" : "ok"}"></span><div><strong>${text}</strong><small class="mono wrap-text">flash:/config/${bus}/${esc(detail.file_name)}</small></div></div><div class="actions"><button data-discard ${!dirty || busy ? "disabled" : ""}>Отбросить черновик</button><button data-apply class="${dirty ? "primary" : ""}" ${!dirty || invalid || busy ? "disabled" : ""}>Применить в RAM</button><button data-save class="primary" ${dirty || detail.saved === true || busy ? "disabled" : ""}>Сохранить во Flash</button></div>`;
  }
  function updateBar() {
    const bar = root.querySelector(".save-bar"); if (bar) { bar.innerHTML = saveBar(); wireSave(); }
  }
  async function mutation(kind) {
    if (busy) return;
    if (kind === "apply" && draft.policy === "whitelist" && !draft.whitelist.length &&
        !await ask({title:"Запретить все записи?",body:"<p>Пустой whitelist заблокирует все обычные записи через сервис устройства.</p>",submit:"Применить",danger:true})) return;
    try {
      await locked(async () => {
      await api.busAction(bus, selected, kind, {revision:detail.revision}, kind === "apply" ? draft : undefined);
      await load(true);
      });
      notice(kind === "save" ? "Снимок конфигурации записан во Flash" : "Изменения применены в RAM. Для сохранения после перезапуска нажмите «Сохранить во Flash».");
    } catch (e) { notice(e.status === 409 ? "Конфигурация изменена другим клиентом. Получите актуальное состояние; черновик пока сохранён в браузере." : e.message, true); }
  }
  function wireSave() {
    root.querySelector("[data-apply]")?.addEventListener("click", () => mutation("apply"));
    root.querySelector("[data-save]")?.addEventListener("click", () => mutation("save"));
    root.querySelector("[data-discard]")?.addEventListener("click", () => { resetDraft(); render(); });
  }
  function content() {
    if (tab === "overview") return `<article class="card"><h2>Устройство ${esc(selected)}</h2>${kv([["Подсистема",label],["Адрес",hex(mdio ? draft.phy_addr : draft.addr_7b)],["Регистров",draft.reg_count],["Политика",detail.config.policy],["Разрешающих правил",detail.config.whitelist.length],["Запрещающих правил",detail.config.blacklist.length]])}</article>${mdio && detail.stats ? `<article class="card"><h2>Счётчики подсистемы MDIO</h2>${kv([["Чтения",detail.stats.reads],["Переданные записи",detail.stats.writes],["Отказы политики",detail.stats.denied],["Ошибки",detail.stats.errors],["Обработанные входные ячейки",detail.stats.observed]])}<p class="muted">Общие счётчики всех PHY. PL может объединять входные записи; точное число потерянных кадров неизвестно.</p></article>` : ""}`;
    if (tab === "policy") return '<div id="policy-workspace"></div>';
    if (tab === "registers") return '<div id="register-workspace"></div>';
    if (tab === "telemetry" || tab === "events") return '<div id="observation-workspace"></div>';

    return "";
  }
  function render() {
    stopObservation();
    ctx.setContext({name:selected,tab});
    const state = deviceState(bus,detail);
    root.innerHTML = `<div class="page-head"><div class="device-heading"><h1>${label}</h1><span class="device-name">${esc(selected)}</span></div></div><div class="device-meta"><span>Адрес <strong class="mono">${hex(mdio ? detail.config.phy_addr : detail.config.addr_7b)}</strong></span><span>Регистров <strong>${draft.reg_count}</strong></span>${badge(state.label,state.ready ? "ok" : "")}</div><div class="tabs" aria-label="Разделы устройства">${Object.entries(tabs).map(([key,title]) => `<button data-tab="${key}" class="${key === tab ? "active" : ""}">${title}</button>`).join("")}</div><div class="stack">${content()}</div>${tab !== "registers" || changed() ? `<div class="save-bar">${saveBar()}</div>` : ""}${fresh}`;
    if (tab === "registers") registerWorkspace(root.querySelector("#register-workspace"), {bus,detail,reads,state:regState,canOperate:!changed(),onRead:reg => registerOp(reg,false),onWrite:(reg,val) => registerOp(reg,true,val)});
    if (tab === "telemetry" || tab === "events") {
      const address = mdio ? detail.config.phy_addr : detail.config.addr_7b, key = `${selected}:${address}:${tab}`;
      if (!observations.has(key)) observations.set(key, {});
      stopObservation = observationWorkspace(root.querySelector("#observation-workspace"), {bus,address,channel:tab,signal:ctx.signal,state:observations.get(key)});
      if (!changed()) root.querySelector(".save-bar")?.remove();
    }
    if (tab === "policy") policyWorkspace(root.querySelector("#policy-workspace"), {bus,base:detail.config,draft,ids:ruleIds,state:ruleState,onChange:value => { invalid = value; updateBar(); },onAdd:key => { if (invalid) { notice("Сначала исправьте неверные значения полей",true); return; } editRule(key); }});
    wireSave();
    root.querySelectorAll("[data-tab]").forEach(b => b.onclick = () => { if (invalid) { notice("Сначала исправьте неверные значения полей",true); return; } clearNotice(); tab = b.dataset.tab; render(); });


  }
  async function editRule(spec) {
    const [key, index] = spec.split(":"), i = index == null ? -1 : Number(index);
    if (i < 0 && draft[key].length >= (mdio ? 64 : 256)) { notice("Достигнут предел количества строк",true); return; }
    const row = i < 0 ? {reg:0,val:0,mask:65535} : draft[key][i];
    await ask({title:"Правило записи",submit:"Готово",body:`<label>Регистр<input name="reg" value="${hex(row.reg)}" required></label><label>Значение<input name="val" value="${hex(row.val,mdio ? 4 : 2)}" required></label>${mdio ? `<label>Маска<input name="mask" value="${hex(row.mask,4)}" required></label><small>0x0000 допускает любое значение. 0xFFFF — точное совпадение.</small>` : ""}`,run:async form => {
      const next = {reg:number(form.elements.reg.value,maxReg(),"Регистр"),val:number(form.elements.val.value,maxValue())};
      if (mdio) next.mask = number(form.elements.mask.value,65535,"Маска");
      if (i < 0) { draft[key].push(next); if (ruleIds[key]) { ruleIds[key].push(-1); ruleState.index = draft[key].length-1; ruleState.list = key; } } else draft[key][i] = next; render();
    }});
  }
  async function registerOp(reg, write, val) {
    if (changed()) throw new Error("Сначала примените или отбросьте черновик конфигурации");
    if (write && !await ask({title:"Записать регистр?",body:`<p>${esc(selected)}: регистр <code>${hex(reg)}</code>, значение <code>${hex(val,mdio ? 4 : 2)}</code>.</p><p>Операция проверяется активной политикой. Запись не повторяется автоматически.</p>`,submit:"Записать"})) return;
    await locked(async () => {
      const result = await api.busAction(bus,selected,write ? "write" : "read",{reg,val,confirm:write ? true : undefined});
      if (!write) { reads.set(reg,{reg,val:result.val,valid:true,source:"Чтение",at:Date.now()}); regState.value = hex(result.val,mdio ? 4 : 2); }
      else reads.delete(reg);
      regState.result = write ? (mdio ? "Кадр передан" : "Запись выполнена") : `${hex(result.val,mdio ? 4 : 2)} (${result.val})`;
      await load(true);
    });
  }
  await load(); return {refresh:load,poll:true,dirty:() => changed() || !!busy,busy:() => !!busy};
}
