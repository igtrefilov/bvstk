import {esc, hex, badge, bits, kv, number, notice, action, wireRows} from "./ui.js";

// Presentation only: selecting a row or refreshing this snapshot never reads PHY.
export function registerWorkspace(root, {bus, detail, reads, state, canOperate, onRead, onWrite, onSnapshot}) {
  const mdio = bus === "mdio", config = detail.config;
  const maxValue = mdio ? 65535 : config.max_value_code, width = mdio ? 4 : 2;
  if (!Number.isInteger(state.reg) || state.reg >= config.reg_count) state.reg = mdio ? Math.min(2,config.reg_count-1) : 0;
  const entry = reg => mdio ? detail.cache?.find(e => e.reg === reg) : reads.get(reg);
  const allowed = (reg,val) => {
    const matches = rules => rules.some(rule => rule.reg === reg && (mdio ? (val & rule.mask) === (rule.val & rule.mask) : val === rule.val));
    return config.policy === "whitelist" ? matches(config.whitelist) : !matches(config.blacklist);
  };
  function select(reg) {
    state.reg = reg; const item = entry(reg);
    state.value = hex(item?.valid ? item.val : 0,width); state.result = "";
    draw();
  }
  function draw() {
    const item = entry(state.reg), value = item?.valid ? item.val : null;
    if (state.value == null) state.value = hex(value ?? 0,width);
    const rows = Array.from({length:config.reg_count},(_,reg) => ({reg,...entry(reg)})).filter(e => state.filter !== "valid" || e.valid);
    root.innerHTML = `<div class="workbench register-workbench"><article class="card"><div class="toolbar"><strong>${mdio ? "Снимок кэша" : "Снимок чтений"}</strong><button data-snapshot>Обновить снимок</button><select class="device-select" aria-label="Фильтр регистров"><option value="all">Все регистры</option><option value="valid" ${state.filter === "valid" ? "selected" : ""}>С данными</option></select></div>
      <div class="table-scroll register-table"><table aria-label="Регистры устройства"><thead><tr><th>Регистр</th><th>Значение</th><th>Достоверность</th><th>Источник</th><th>Возраст</th></tr></thead><tbody>${rows.map(e => `<tr class="selectable ${e.reg === state.reg ? "selected" : ""}" tabindex="0" data-register="${e.reg}"><td class="mono">${hex(e.reg)}</td><td class="mono">${e.valid ? hex(e.val,width) : "—"}</td><td>${badge(e.valid ? "Достоверно" : "Нет данных",e.valid ? "ok" : "")}</td><td>${e.valid ? esc(e.source || "Чтение") : "—"}</td><td>${e.valid ? esc(e.at ? Math.max(0,Date.now()-e.at) : e.age_ms) + " мс" : "—"}</td></tr>`).join("") || '<tr><td colspan="5" class="empty">Нет данных</td></tr>'}</tbody></table></div>
      ${mdio && detail.stats ? `<div class="counter-strip">${[["Чтения",detail.stats.reads],["Записи",detail.stats.writes],["Отказы политики",detail.stats.denied],["Ошибки",detail.stats.errors]].map(([label,n]) => `<div>${label}<strong>${esc(n)}</strong></div>`).join("")}</div>` : ""}<p class="muted">${mdio ? "Обновление снимка не запускает чтение всех регистров. Возраст указан на момент получения снимка." : "Показаны только результаты явного чтения в этом сеансе. Обновление не читает регистры устройства."}</p></article>
      <aside class="card inspector"><h2>Регистр ${hex(state.reg)}</h2><small>Значение HEX</small><div class="value mono">${value == null ? "—" : hex(value,width)}</div>${kv([["Десятичное",value ?? "—"],["Источник",value == null ? "—" : item.source || "Чтение"],["Достоверность",value == null ? "Нет данных" : "Достоверно"]])}<div class="subsection"><small>Битовое представление</small>${bits(value,mdio ? 16 : 8)}</div>
      <div class="subsection"><h3>Операция с регистром</h3><div class="form-grid"><label>Регистр<input id="reg" class="mono" value="${hex(state.reg)}" autocomplete="off"></label><label>Значение<input id="val" class="mono" value="${esc(state.value)}" autocomplete="off"></label></div><div class="actions"><button data-read>Прочитать</button><button data-write>Записать</button></div><p class="warning-text" id="write-policy" role="status"></p><p id="reg-result" class="mono" role="status">${esc(state.result || "")}</p><p class="muted">${mdio ? "Таблица показывает кэш. Команда чтения обращается к PHY." : "Команда чтения обращается к устройству."} Чтение некоторых регистров меняет их состояние.</p>${mdio ? '<small>Успех записи означает передачу кадра, не подтверждение PHY.</small>' : ""}</div></aside></div>`;
    wireRows(root,"[data-register]",row => select(Number(row.dataset.register)));
    root.querySelector("[data-snapshot]").onclick = onSnapshot;
    root.querySelector("select").onchange = event => { state.filter = event.target.value; draw(); };
    root.querySelector("#reg").onchange = event => {
      try { select(number(event.target.value,config.reg_count-1,"Регистр")); }
      catch(e) { event.target.setCustomValidity(e.message); updateWrite(); notice(e.message,true); }
    };
    root.querySelector("#reg").oninput = () => updateWrite();
    root.querySelector("#val").oninput = event => { state.value = event.target.value; updateWrite(); };
    root.querySelector("[data-read]").onclick = event => action(event.currentTarget,async () => {
      const reg = number(root.querySelector("#reg").value,config.reg_count-1,"Регистр");
      state.reg = reg; await onRead(reg);
    });
    root.querySelector("[data-write]").onclick = event => action(event.currentTarget,async () => {
      const reg = number(root.querySelector("#reg").value,config.reg_count-1,"Регистр");
      const val = number(state.value,maxValue);
      if (!allowed(reg,val)) throw new Error("Запись запрещена текущим снимком политики");
      state.reg = reg; await onWrite(reg,val);
    });
    updateWrite();
  }
  function updateWrite() {
    let reason = "", validReg = true;
    const field = root.querySelector("#reg");
    let reg;
    try { reg = number(field.value,config.reg_count-1,"Регистр"); field.setCustomValidity(""); }
    catch(e) { reason = e.message; validReg = false; field.setCustomValidity(reason); }
    try {
      const val = number(state.value,maxValue);
      if (validReg && !allowed(reg,val)) reason = "Запись запрещена политикой";
    } catch(e) { reason = e.message; }
    if (!canOperate) reason = "Сначала примените или отбросьте черновик конфигурации";
    root.querySelector("[data-read]").disabled = !canOperate || !validReg;
    root.querySelector("[data-write]").disabled = !!reason;
    root.querySelector("#write-policy").textContent = reason;
  }
  draw();
}
