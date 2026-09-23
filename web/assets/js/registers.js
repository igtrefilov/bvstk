import {esc, hex, bits, number, notice, action, wireRows} from "./ui.js";

function focusWithoutScroll(element) {
  element?.focus?.({preventScroll:true});
}

function uptimeLabel(value) {
  const milliseconds = Number(value);
  if (!Number.isFinite(milliseconds) || milliseconds < 0) return "—";
  const whole = Math.floor(milliseconds);
  const seconds = Math.floor(whole / 1000);
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const remainder = seconds % 60;
  return `T+${String(hours).padStart(2,"0")}:${String(minutes).padStart(2,"0")}:${String(remainder).padStart(2,"0")}.${String(whole % 1000).padStart(3,"0")}`;
}

function metadataRows(value, item) {
  return [["Десятичное",value ?? "—"],["Последнее обновление",value == null ? "—" : uptimeLabel(item.updated_ms)]];
}
function metadataMarkup(value, item) {
  return `<dl class="kv register-meta">${metadataRows(value,item).map(([key,entry]) => `<dt>${esc(key)}</dt><dd>${esc(entry)}</dd>`).join("")}</dl>`;
}

// Presentation only: selecting a row never reads PHY.
export function registerWorkspace(root, {bus, detail, state, canOperate, onRead, onWrite}) {
  const mdio = bus === "mdio", config = detail.config;
  const maxValue = mdio ? 65535 : config.max_value_code, width = mdio ? 4 : 2;
  if (!Number.isInteger(state.reg) || state.reg >= config.reg_count) state.reg = mdio ? Math.min(2,config.reg_count-1) : 0;
  const entry = reg => detail.cache?.find(e => e.reg === reg);
  let pointerActive = false;
  const allowed = (reg,val) => {
    const matches = rules => rules.some(rule => rule.reg === reg && (mdio ? (val & rule.mask) === (rule.val & rule.mask) : val === rule.val));
    return config.policy === "whitelist" ? matches(config.whitelist) : !matches(config.blacklist);
  };
  function wireRegisterRows() {
    wireRows(root,"[data-register]",row => {
      select(Number(row.dataset.register),true);
    });
  }
  function wireTableInteraction() {
    const table = root.querySelector(".register-table");
    if (!table) return;
    table.addEventListener("pointerdown",() => {
      pointerActive = true;
    });
    const releasePointer = () => { pointerActive = false; };
    table.addEventListener("pointerup",releasePointer);
    table.addEventListener("pointercancel",releasePointer);
  }
  function select(reg, focusRow = false) {
    const table = root.querySelector(".register-table");
    const scrollTop = table?.scrollTop ?? 0, scrollLeft = table?.scrollLeft ?? 0;
    state.reg = reg; state.valueDirty = false; const item = entry(reg);
    state.value = hex(item?.valid ? item.val : 0,width); state.result = "";
    draw();
    const nextTable = root.querySelector(".register-table");
    if (focusRow) focusWithoutScroll(root.querySelector(`[data-register="${reg}"]`));
    if (nextTable) { nextTable.scrollTop = scrollTop; nextTable.scrollLeft = scrollLeft; }
  }
  function draw() {
    const item = entry(state.reg), value = item?.valid ? item.val : null;
    if (state.value == null || !state.valueDirty) state.value = hex(value ?? 0,width);
    const rows = visibleRows();
    root.innerHTML = `<div class="workbench register-workbench"><article class="card"><div class="toolbar"><strong>Снимок кэша</strong><select class="device-select" aria-label="Фильтр регистров"><option value="all">Все регистры</option><option value="valid" ${state.filter === "valid" ? "selected" : ""}>С данными</option></select></div>
      <div class="table-scroll register-table"><table aria-label="Регистры устройства"><thead><tr><th>Регистр</th><th>Значение в кэше</th><th>Последнее обновление</th></tr></thead><tbody>${rowMarkup(rows)}</tbody></table></div>
      ${mdio && detail.stats ? `<div class="counter-strip">${[["Чтения",detail.stats.reads],["Записи",detail.stats.writes],["Отказы политики",detail.stats.denied],["Ошибки",detail.stats.errors]].map(([label,n]) => `<div>${label}<strong>${esc(n)}</strong></div>`).join("")}</div>` : ""}<p class="muted">Кэш обновляется автоматически после физического обмена с устройством. Новые значения появляются без повторного чтения из браузера.</p></article>
      <aside class="card inspector"><h2>Регистр ${hex(state.reg)}</h2><small>Значение HEX</small><div class="value mono">${value == null ? "—" : hex(value,width)}</div>${metadataMarkup(value,item)}<div class="subsection"><small>Битовое представление</small>${bits(value,mdio ? 16 : 8)}</div>
      <div class="subsection"><h3>Операция с регистром</h3><div class="form-grid"><label>Регистр<input id="reg" class="mono" value="${hex(state.reg)}" autocomplete="off"></label><label>Значение<input id="val" class="mono" value="${esc(state.value)}" autocomplete="off"></label></div><div class="actions"><button data-read>Прочитать</button><button data-write>Записать</button></div><p class="warning-text" id="write-policy" role="status"></p><p id="reg-result" class="mono" role="status">${esc(state.result || "")}</p><p class="muted">${mdio ? "Таблица показывает кэш. Команда чтения обращается к PHY." : "Команда чтения обращается к устройству."} Чтение некоторых регистров меняет их состояние.</p>${mdio ? '<small>Успех записи означает передачу кадра, не подтверждение PHY.</small>' : ""}</div></aside></div>`;
    wireTableInteraction();
    wireRegisterRows();
    root.querySelector("select").onchange = event => { state.filter = event.target.value; refresh(); };
    root.querySelector("#reg").onchange = event => {
      try { select(number(event.target.value,config.reg_count-1,"Регистр")); }
      catch(e) { event.target.setCustomValidity(e.message); updateWrite(); notice(e.message,true); }
    };
    root.querySelector("#reg").oninput = () => updateWrite();
    root.querySelector("#val").oninput = event => { state.value = event.target.value; state.valueDirty = true; updateWrite(); };
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
  function visibleRows() {
    return Array.from({length:config.reg_count},(_,reg) => ({reg,...entry(reg)}))
      .filter(e => state.filter !== "valid" || e.valid);
  }
  function rowMarkup(rows) {
    return rows.map(e => `<tr class="selectable ${e.reg === state.reg ? "selected" : ""}" tabindex="0" data-register="${e.reg}"><td class="mono">${hex(e.reg)}</td><td class="mono">${e.valid ? hex(e.val,width) : "—"}</td><td>${e.valid ? esc(uptimeLabel(e.updated_ms)) : "—"}</td></tr>`).join("") || '<tr><td colspan="3" class="empty">Нет данных</td></tr>';
  }
  function updateRows(body, rows) {
    const current = [...body.querySelectorAll("[data-register]")];
    const sameShape = current.length === rows.length && rows.every((item,index) => Number(current[index].dataset.register) === item.reg);
    if (!sameShape) {
      body.innerHTML = rowMarkup(rows);
      wireRegisterRows();
      return "replaced";
    }
    current.forEach((row,index) => {
      const item = rows[index], cells = row.children;
      row.classList.toggle("selected",item.reg === state.reg);
      if (cells[1]) cells[1].textContent = item.valid ? hex(item.val,width) : "—";
      if (cells[2]) cells[2].textContent = item.valid ? uptimeLabel(item.updated_ms) : "—";
    });
    return "in-place";
  }
  function refresh() {
    if (pointerActive) {
      return;
    }
    const table = root.querySelector(".register-table"), body = table?.querySelector("tbody");
    if (!table || !body) { draw(); return; }
    const scrollTop = table.scrollTop, scrollLeft = table.scrollLeft;
    const active = document.activeElement, activeRegister = active?.closest?.("[data-register]")?.dataset.register;
    updateRows(body,visibleRows());
    const item = entry(state.reg), value = item?.valid ? item.val : null;
    if (!state.valueDirty) {
      state.value = hex(value ?? 0,width);
      const valueInput = root.querySelector("#val");
      if (valueInput) valueInput.value = state.value;
    }
    const valueOutput = root.querySelector(".register-workbench .inspector .value");
    if (valueOutput) valueOutput.textContent = value == null ? "—" : hex(value,width);
    const meta = root.querySelector(".register-workbench .register-meta");
    if (meta) meta.innerHTML = metadataRows(value,item).map(([key,entryValue]) => `<dt>${esc(key)}</dt><dd>${esc(entryValue)}</dd>`).join("");
    const bitGrid = root.querySelector(".register-workbench .bit-grid");
    if (bitGrid) {
      const template = document.createElement("template"); template.innerHTML = bits(value,mdio ? 16 : 8);
      bitGrid.replaceWith(template.content.firstElementChild);
    }
    if (detail.stats) {
      const values = [detail.stats.reads,detail.stats.writes,detail.stats.denied,detail.stats.errors];
      root.querySelectorAll(".register-workbench .counter-strip strong").forEach((node,index) => { node.textContent = values[index] ?? "—"; });
    }
    if (activeRegister !== undefined) focusWithoutScroll(root.querySelector(`[data-register="${activeRegister}"]`));
    table.scrollTop = scrollTop; table.scrollLeft = scrollLeft;
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
  return {refresh};
}
