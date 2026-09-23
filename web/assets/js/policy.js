import {esc, hex, number, icon, wireRows} from "./ui.js";

export function ruleChanges(base, draft, ids) {
  const changes = [];
  for (const key of ["whitelist","blacklist"]) {
    draft[key].forEach((row,index) => {
      const previous = ids[key][index] < 0 ? null : base[key][ids[key][index]];
      if (!previous || JSON.stringify(previous) !== JSON.stringify(row)) changes.push({key,index,kind:previous ? "Изменено" : "Добавлено",previous,row});
    });
    base[key].forEach((row,index) => { if (!ids[key].includes(index)) changes.push({key,index,kind:"Удалено",previous:row}); });
  }
  return changes;
}
export function policyWorkspace(root, {bus, base, draft, ids, state, onChange, onAdd}) {
  const mdio = bus === "mdio", width = mdio ? 4 : 2;
  const condition = row => {
    if (!mdio) return "Точное значение";
    if (row.mask === 0) return "Любое значение";
    if (row.mask === 65535) return "Точное совпадение";
    if ((row.mask & (row.mask-1)) === 0) return `Бит ${Math.log2(row.mask)} ${row.val & row.mask ? "установлен" : "сброшен"}`;
    return "По маске";
  };
  const changes = () => ruleChanges(base,draft,ids);
  function summary() {
    const count = changes().length, mode = draft.policy !== base.policy;
    return count || mode ? `${count ? "Изменений правил: " + count + ". " : ""}${mode ? "Изменён режим политики. " : ""}Черновик ещё не применён.` : "Правила совпадают с рабочим снимком RAM";
  }
  function table() {
    const diffs = changes();
    return `<div class="table-scroll"><table aria-label="Правила политики"><thead><tr><th>Регистр</th><th>Значение</th>${mdio ? "<th>Маска</th>" : ""}<th>Условие</th><th>Изменение</th><th></th></tr></thead><tbody>${draft[state.list].map((row,i) => {
      const diff = diffs.find(d => d.key === state.list && d.index === i && d.kind !== "Удалено");
      return `<tr class="selectable ${i === state.index ? "selected" : ""} ${diff ? "changed" : ""}" tabindex="0" data-rule="${i}"><td class="mono">${hex(row.reg)}</td><td class="mono">${hex(row.val,width)}</td>${mdio ? `<td class="mono">${hex(row.mask,4)}</td>` : ""}<td>${condition(row)}</td><td class="${diff ? "warning-text" : ""}">${diff?.kind || "—"}</td><td class="actions-cell"><button class="text-button" data-rule-edit="${i}" aria-label="Изменить строку ${i+1}">${icon("edit")}</button><button class="text-button" data-delete="${state.list}:${i}" aria-label="Удалить строку ${i+1}">${icon("trash")}</button></td></tr>`;
    }).join("") || `<tr><td colspan="${mdio ? 6 : 5}" class="empty">Список пуст</td></tr>`}</tbody></table></div>`;
  }
  function comparison() {
    const row = draft[state.list][state.index]; if (!row) return "";
    const previous = ids[state.list][state.index] < 0 ? null : base[state.list][ids[state.list][state.index]];
    return `<h3>Сравнение с RAM</h3><table class="rule-compare"><thead><tr><th>Поле</th><th>В RAM</th><th>Черновик</th></tr></thead><tbody>${[["reg","Регистр"],["val","Значение"],...(mdio ? [["mask","Маска"]] : [])].map(([key,label]) => `<tr><td>${label}</td><td class="mono">${previous ? hex(previous[key],key === "reg" ? 2 : width) : "—"}</td><td class="mono ${!previous || previous[key] !== row[key] ? "modified" : ""}">${hex(row[key],key === "reg" ? 2 : width)}</td></tr>`).join("")}</tbody></table>`;
  }
  function inspector() {
    const row = draft[state.list][state.index];
    if (!row) return '<h2>Правило</h2><div class="empty">Выберите или добавьте правило</div>';
    return `<h2 id="rule-title">Правило ${hex(row.reg)}</h2><div class="rule-fields">${[["reg","Регистр"],["val","Значение"],...(mdio ? [["mask","Маска"]] : [])].map(([key,label]) => `<label>${label}<input class="mono" data-rule-field="${key}" value="${hex(row[key],key === "reg" ? 2 : width)}" autocomplete="off" required></label>`).join("")}</div><p id="rule-condition">${condition(row)}</p><div class="subsection" id="rule-compare">${comparison()}</div><div class="actions"><button data-remove-rule>${icon("trash")}Удалить правило</button></div>`;
  }
  function select(index) {
    if (root.querySelector("input:invalid")) { root.querySelector("input:invalid").reportValidity(); return; }
    state.index = index; draw();
  }
  function wireTable() {
    wireRows(root,"[data-rule]",row => select(Number(row.dataset.rule)));
    root.querySelectorAll("[data-rule-edit]").forEach(b => b.onclick = () => select(Number(b.dataset.ruleEdit)));
    root.querySelectorAll("[data-delete]").forEach(b => b.onclick = () => remove(Number(b.dataset.delete.split(":")[1])));
  }
  function remove(index) {
    draft[state.list].splice(index,1); ids[state.list].splice(index,1);
    state.index = Math.min(state.index,draft[state.list].length-1); draw(); onChange(false);
  }
  function update() {
    const notice = root.querySelector("#rule-summary"); notice.textContent = summary();
    notice.className = (changes().length || draft.policy !== base.policy ? "warning-strip" : "info-strip") + " rule-summary";
    root.querySelector("#rules-table").innerHTML = table(); wireTable();
    root.querySelector("#deleted-rules").innerHTML = changes().filter(c => c.kind === "Удалено").map(c => `<li>Удалено ${c.key}: регистр ${hex(c.previous.reg)}, значение ${hex(c.previous.val,width)}</li>`).join("");
    const compare = root.querySelector("#rule-compare"); if (compare) compare.innerHTML = comparison();
    const row = draft[state.list][state.index];
    if (row) { root.querySelector("#rule-title").textContent = "Правило " + hex(row.reg); root.querySelector("#rule-condition").textContent = condition(row); }
  }
  function draw() {
    if (!["whitelist","blacklist"].includes(state.list)) state.list = "whitelist";
    state.index = Math.max(0,Math.min(state.index || 0,draft[state.list].length-1));
    root.innerHTML = `<div id="rule-summary" class="info-strip rule-summary"></div><article class="card" style="margin-bottom:12px"><div class="radio-row"><strong>Политика записи</strong>${["whitelist","blacklist"].map(mode => `<label class="check"><input type="radio" name="policy" value="${mode}" ${draft.policy === mode ? "checked" : ""}>${mode}</label>`).join("")}<span class="muted" id="policy-mode-help"></span></div></article><div class="workbench"><div class="stack"><article class="card"><div class="toolbar"><div class="tabs compact">${["whitelist","blacklist"].map(key => `<button data-list="${key}" class="${state.list === key ? "active" : ""}">${key === "whitelist" ? "Whitelist" : "Blacklist"} ${draft[key].length}</button>`).join("")}</div><button data-add-rule>+ Добавить правило</button></div><div id="rules-table">${table()}</div><ul class="change-list" id="deleted-rules"></ul></article><article class="card"><h3>Проверка правила</h3>${mdio ? '<code>(value &amp; mask) == (rule &amp; mask)</code><p class="muted">Маска 0x0000 допускает любое значение.<br>Маска 0xFFFF требует точного совпадения.</p>' : '<p>Правило проверяет точное совпадение регистра и значения.</p>'}<p class="muted">Пустой whitelist запрещает все записи.</p></article></div><aside class="card inspector">${inspector()}</aside></div>`;
    const modeHelp = () => { root.querySelector("#policy-mode-help").textContent = draft.policy === "whitelist" ? "Разрешать только совпавшие правила" : "Запрещать совпавшие правила"; };
    root.querySelectorAll("[name=policy]").forEach(input => input.onchange = () => { draft.policy = input.value; modeHelp(); update(); onChange(!!root.querySelector("input:invalid")); });
    root.querySelectorAll("[data-list]").forEach(button => button.onclick = () => { if (root.querySelector("input:invalid")) { root.querySelector("input:invalid").reportValidity(); return; } state.list = button.dataset.list; state.index = 0; draw(); });
    root.querySelector("[data-add-rule]").onclick = () => onAdd(state.list);
    root.querySelector("[data-remove-rule]")?.addEventListener("click",() => remove(state.index));
    root.querySelectorAll("[data-rule-field]").forEach(input => input.oninput = () => {
      try {
        const key = input.dataset.ruleField, max = key === "reg" ? draft.reg_count-1 : mdio ? 65535 : draft.max_value_code;
        draft[state.list][state.index][key] = number(input.value,max,key === "reg" ? "Регистр" : key === "mask" ? "Маска" : "Значение");
        input.setCustomValidity(""); update();
      } catch(e) { input.setCustomValidity(e.message); }
      onChange(!!root.querySelector("input:invalid"));
    });
    modeHelp(); update();
  }
  draw();
}
