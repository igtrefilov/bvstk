import * as api from "./api.js";
import {esc, hex} from "./ui.js";

const sources = ["Консоль", "Внешний мастер", "DCP2", "Система", "Web"];
const kinds = {1:"Попытка записи",2:"Запись выполнена",3:"Запись запрещена",4:"Изменение состояния",5:"Ошибка записи",
  257:"Перехваченная запись",258:"Чтение master",259:"Запись master",260:"Указатель регистра",
  261:"Запрос чтения slave",262:"Подготовлен ответ slave"};
const statuses = ["Успешно","Неверный запрос","Не поддерживается","Запрещено","Занято","Таймаут",
  "Вне диапазона","Не готово или пропущено","Не найдено","Ошибка обмена","Внутренняя ошибка"];
const maxRows = 512;
function time(us) {
  const ms = Math.floor(us / 1000), seconds = Math.floor(ms / 1000);
  return `${Math.floor(seconds / 3600).toString().padStart(2,"0")}:${Math.floor(seconds / 60 % 60).toString().padStart(2,"0")}:${(seconds % 60).toString().padStart(2,"0")}.${(ms % 1000).toString().padStart(3,"0")}`;
}

export function observationWorkspace(root, {bus, address, channel, signal, state}) {
  const events = channel === "events", title = events ? "События" : "Телеметрия";
  const terminalEvent = type => type === 2 || type === 3 || type === 5;
  Object.assign(state, {rows:state.rows || [], lost:state.lost || 0n, trimmed:state.trimmed || 0,
    source:state.source ?? "", kind:state.kind ?? "", reg:state.reg ?? ""});
  if (events && state.kind && !terminalEvent(Number(state.kind))) state.kind = "";
  const controller = new AbortController();
  let timer, stopped = false, failure = "", frozen = state.paused ? [...state.rows] : null;
  const stop = () => { stopped = true; clearTimeout(timer); controller.abort(); signal.removeEventListener("abort",stop); };
  signal.addEventListener("abort", stop, {once:true});
  if (signal.aborted) { stop(); return stop; }
  const types = Object.entries(kinds).filter(([type]) => events ? terminalEvent(Number(type)) : Number(type) >= 256);
  root.innerHTML = `<article class="card observation-card"><div class="card-head"><h2>${title}</h2><div class="actions"><button data-live-pause>${state.paused ? "Продолжить" : "Пауза"}</button><button data-live-clear>Очистить экран</button></div></div>
    <div class="observation-filters"><label>Источник<select data-live-source><option value="">Все источники</option>${sources.map((name,i)=>`<option value="${i}" ${state.source === String(i) ? "selected" : ""}>${name}</option>`).join("")}</select></label>
    <label>${events ? "Событие" : "Операция"}<select data-live-kind><option value="">Все</option>${types.map(([type,name])=>`<option value="${type}" ${state.kind === type ? "selected" : ""}>${name}</option>`).join("")}</select></label>
    <label>Регистр<input data-live-reg value="${esc(state.reg)}" placeholder="Все регистры" inputmode="text"></label></div>
    <p class="observation-state" data-live-state role="status">Подключение к журналу…</p>
    <p class="info-strip observation-warning" data-live-warning hidden></p>
    <div class="table-scroll observation-table"><table><thead><tr><th>Время работы</th><th>Источник</th><th>${events ? "Событие" : "Операция"}</th><th>Адрес</th><th>Регистр</th><th>Значение</th><th>Результат</th></tr></thead><tbody data-live-rows></tbody></table></div>
    <p class="muted observation-note">${events ? "Показываются только итоговые записи: выполненные, запрещённые и ошибочные. " : "Время регистрации в PS, не аппаратная метка кадра. Просмотр не запускает чтения устройства. "}${bus === "mdio" ? "PL хранит последние записи; потери и порядок кадров на проводе неизвестны. Чтения внешнего мастера из кэша slave не наблюдаются. Успех записи означает передачу кадра, не подтверждение PHY." : "Подготовленный ответ slave не доказывает чтение всех байтов внешним мастером. Наблюдается только выбранный аппаратный slave; профиль не создаёт дополнительный канал перехвата."}</p>
    <p class="muted observation-note">История хранится в RAM: 256 записей на шину для каждого журнала. В браузере — последние 512 строк. Во Flash не сохраняется.</p></article>`;
  const body = root.querySelector("[data-live-rows]"), status = root.querySelector("[data-live-state]"), warning = root.querySelector("[data-live-warning]");
  function paint() {
    if (stopped) return;
    status.classList.toggle("error-text",!!failure);
    status.textContent = failure ? `Обновление недоступно: ${failure}. Повторное подключение…` :
      `${state.paused ? "Отображение приостановлено, приём продолжается" : "Обновляется автоматически"}. Получено строк: ${state.rows.length}`;
    const messages = [];
    if (state.lost) messages.push(`Пропущено в программном журнале шины: ${state.lost}. Счётчик до фильтра по устройству, не число потерянных кадров PL.`);
    if (state.trimmed) messages.push(`Из окна браузера вытеснено строк: ${state.trimmed}.`);
    if (state.restarted) messages.push("Система перезапущена или журнал сброшен; предыдущая история очищена.");
    warning.hidden = !messages.length; warning.textContent = messages.join(" ");
    const query = state.reg.trim(), register = query ? Number(query) : null;
    const valid = !query || /^(?:0x[0-9a-f]+|[0-9]+)$/i.test(query) && Number.isInteger(register) && register <= 255;
    root.querySelector("[data-live-reg]").setCustomValidity(valid ? "" : "Введите номер регистра, например 0x02");
    const rows = (state.paused ? frozen : state.rows).filter(r => valid && (!events || terminalEvent(r.type)) &&
      (state.source === "" || r.source === Number(state.source)) &&
      (state.kind === "" || r.type === Number(state.kind)) && (register === null || r.reg === register));
    body.innerHTML = [...rows].reverse().map(r => {
      const noValue = r.type === 261 || (r.type === 258 && r.status !== 0);
      let result = statuses[r.status] || `Код ${r.status}`;
      if (r.type === 257 || r.type === 260 || r.type === 261) result = "Наблюдается PS";
      if (r.type === 262) result = "Подготовлено, не подтверждено";
      if (r.type === 2) result = r.status === 0 ? "Запись выполнена" : "Ошибка записи";
      if (r.type === 3) result = r.status === 3 ? "Запись запрещена" : "Ошибка записи";
      if (r.type === 5) result = "Ошибка записи";
      const success = r.type === 2 && r.status === 0;
      return `<tr data-record-id="${esc(r.id)}" class="${success ? "observation-success" : "observation-failed"}"><td class="mono">${time(r.time_us)}</td><td>${esc(sources[r.source] || "Неизвестно")}</td><td>${esc(kinds[r.type] || `Код ${r.type}`)}</td><td class="mono">${hex(r.address)}</td><td class="mono">${hex(r.reg)}</td><td class="mono">${noValue ? "—" : hex(r.value,bus === "mdio" ? 4 : 2)}</td><td>${esc(result)}</td></tr>`;
    }).join("") || `<tr><td colspan="7" class="empty">${state.rows.length ? "Нет записей по выбранным фильтрам" : "Ожидание данных устройства"}</td></tr>`;
  }
  for (const key of ["source","kind","reg"]) root.querySelector(`[data-live-${key}]`).addEventListener(key === "reg" ? "input" : "change", event => {state[key] = event.target.value; paint();});
  root.querySelector("[data-live-pause]").onclick = event => {
    state.paused = !state.paused; frozen = state.paused ? [...state.rows] : null;
    event.currentTarget.textContent = state.paused ? "Продолжить" : "Пауза"; paint();
  };
  root.querySelector("[data-live-clear]").onclick = () => {
    state.rows = []; frozen = []; state.lost = 0n; state.trimmed = 0; state.restarted = false; paint();
  };
  async function poll() {
    let delay = 500;
    try {
      if (stopped || document.hidden || api.pending || Date.now() < api.downloadPauseUntil) return;
      const response = await api.get(api.url("/api/ui/observe", {bus,channel,address,after:state.cursor}),controller.signal);
      if (stopped) return;
      if (!Array.isArray(response.records) || !/^\d+$/.test(response.next) || !/^\d+$/.test(response.lost)) throw new Error("Неверный ответ журнала");
      if (response.reset || (state.uptime != null && response.uptime_ms < state.uptime)) {
        state.rows = []; frozen = []; state.lost = 0n; state.trimmed = 0; state.restarted = true;
        // A reboot can already have produced more records than the old cursor.
        if (!response.reset) { state.cursor = undefined; state.uptime = response.uptime_ms; return; }
      }
      state.uptime = response.uptime_ms; state.cursor = response.next;
      state.lost += BigInt(response.lost); state.rows.push(...(events ? response.records.filter(r => terminalEvent(r.type)) : response.records));
      if (state.rows.length > maxRows) { state.trimmed += state.rows.length - maxRows; state.rows.splice(0,state.rows.length-maxRows); }
      failure = ""; paint(); if (response.more) delay = 100;
    } catch (e) { if (!stopped) { failure = e.message; paint(); delay = 2000; } }
    finally { if (!stopped) timer = setTimeout(poll, delay); }
  }
  paint(); poll();
  return stop;
}
