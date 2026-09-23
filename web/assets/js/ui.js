export const esc = value => String(value ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
export const hex = (n, width = 2) => Number.isInteger(n) ? "0x" + n.toString(16).toUpperCase().padStart(width, "0") : "—";
export function bytes(n) {
  if (n == null || !Number.isFinite(n)) return "Нет данных";
  const units = ["Б", "КиБ", "МиБ", "ГиБ"];
  let i = 0; while (n >= 1024 && i < 3) { n /= 1024; i++; }
  return n.toLocaleString("ru-RU", {maximumFractionDigits: i ? 1 : 0}) + " " + units[i];
}
export function uptime(ms) {
  if (!Number.isFinite(ms)) return "—";
  const s = Math.floor(ms / 1000), days = Math.floor(s / 86400);
  return (days ? days + " д. " : "") + [Math.floor(s / 3600) % 24, Math.floor(s / 60) % 60, s % 60].map(n => String(n).padStart(2, "0")).join(":");
}
// Eight identical teeth around the exact center of the 24px view box.
const gearOutline = Array.from({length:8},(_,tooth) =>
  [[-14,7.5],[-9,10],[9,10],[14,7.5]].map(([offset,radius]) => {
    const angle = (tooth * 45 + offset - 90) * Math.PI / 180;
    return `${(12 + Math.cos(angle) * radius).toFixed(3)} ${(12 + Math.sin(angle) * radius).toFixed(3)}`;
  })).flat().map((point,index) => `${index ? "L" : "M"}${point}`).join(" ") + " Z";
const paths = {
  home:'<path d="m3 11 9-8 9 8M5 10v11h5v-7h4v7h5V10"/>',
  grid:'<rect x="3" y="3" width="7" height="7" rx="2"/><rect x="14" y="3" width="7" height="7" rx="2"/><rect x="3" y="14" width="7" height="7" rx="2"/><rect x="14" y="14" width="7" height="7" rx="2"/>',
  bus:'<path d="M2 8H22M2 16H22M6 8V3M18 16V21"/><circle cx="6" cy="3" r="1.5" fill="currentColor" stroke="none"/><circle cx="18" cy="21" r="1.5" fill="currentColor" stroke="none"/>',
  chip:'<rect x="6" y="6" width="12" height="12" rx=".5"/><path d="M9 2V6M15 2V6M9 18V22M15 18V22M2 9H6M2 15H6M18 9H22M18 15H22"/>',
  runtime:'<circle cx="12" cy="12" r="8"/><path d="M12 7v5l3 2M12 2v2M12 20v2M2 12h2M20 12h2"/>',
  disk:'<ellipse cx="12" cy="5" rx="8" ry="3"/><path d="M4 5v14c0 4 16 4 16 0V5M4 12c0 4 16 4 16 0"/>',
  storage:'<path d="M6 3h12l3 13v5H3v-5zM3 16h18M7 7h10M6 11h12M7 19h1"/>',
  tools:'<path d="M14 5a5 5 0 0 0-6 6L2 17l5 5 6-7a5 5 0 0 0 6-6l-4 3-3-3z"/>',
  file:'<path d="M5 2h9l5 5v15H5zM14 2v6h5"/>',
  folder:'<path d="M3 5h7l2 3h9v13H3z"/>',
  settings:`<path d="${gearOutline}"/><circle cx="12" cy="12" r="3"/>`,
  net:'<rect x="9" y="2" width="6" height="5" rx="1"/><path d="M12 7v5M5 12h14M5 12v4m14-4v4"/><rect x="2" y="16" width="6" height="5" rx="1"/><rect x="16" y="16" width="6" height="5" rx="1"/>',
  refresh:'<path d="M20 7a9 9 0 1 0 1 7M20 2v6h-6"/>',
  upload:'<path d="M12 16V3m-5 5 5-5 5 5M3 15v6h18v-6"/>',
  download:'<path d="M12 3v13m-5-5 5 5 5-5M3 16v5h18v-5"/>',
  up:'<path d="m5 10 7-7 7 7M12 3v18"/>',
  edit:'<path d="m4 16-1 5 5-1L21 7l-4-4zM14 6l4 4"/>',
  copy:'<rect x="8" y="8" width="13" height="13"/><path d="M16 8V3H3v13h5"/>',
  trash:'<path d="M3 6h18M9 6V3h6v3M6 6l1 15h10l1-15M10 10v7m4-7v7"/>',
};
export const icon = (name, {ready} = {}) => `<svg class="icon ${name === "folder" ? "folder" : ""}" data-icon-name="${esc(name)}" viewBox="0 0 24 24" aria-hidden="true">${paths[name] || paths.file}${name === "chip" && typeof ready === "boolean" ? `<circle class="device-status${ready ? " ready" : ""}" cx="15" cy="9" r="2" stroke="none"/>` : ""}</svg>`;
export function deviceState(bus, device) {
  // Older I2C firmware reports service readiness as availability. Do not
  // misrepresent that flag as proof that a configured chip has responded.
  const presence = device.presence ?? (bus === "mdio" ? (device.available ? "responding" : "unavailable") : "unknown");
  const ready = presence === "responding";
  return {ready,label:ready ? "Устройство отвечает" : presence === "unavailable" ? "Нет ответа устройства" : "Ответ не проверен"};
}
export function badge(text, state = "") { return `<span class="badge ${state}"><span class="dot ${state}"></span>${esc(text)}</span>`; }
export const kv = rows => `<dl class="kv">${rows.map(([key,value]) => `<dt>${esc(key)}</dt><dd>${esc(value)}</dd>`).join("")}</dl>`;
export const head = (title, subtitle, extra = "") => `<div class="page-head"><div><h1>${esc(title)}</h1><p>${esc(subtitle)}</p></div>${extra ? `<div class="actions">${extra}</div>` : ""}</div>`;
export function footer() {
  const text = "Обновлено " + new Date().toLocaleTimeString("ru-RU");
  const global = document.getElementById("snapshot-status");
  if (global) { global.textContent = text; global.classList.remove("error-text"); }
  return `<div class="page-foot" data-fresh>${text}</div>`;
}
export function stale(root, error) {
  const out = root.querySelector("[data-fresh]");
  if (out) { out.classList.add("error-text"); out.textContent = "Данные устарели. " + error.message; }
  root.classList.add("is-stale");
  const global = document.getElementById("snapshot-status");
  if (global) { global.textContent = "Данные устарели"; global.classList.add("error-text"); }
}
export function bits(value, width = 16) {
  return `<div class="bit-grid" role="img" aria-label="${value == null ? "Нет данных" : "Битовое представление " + hex(value,width / 4)}">${Array.from({length:width},(_,i) => { const bit = width-i-1, set = value == null ? null : (value >>> bit) & 1; return `<div class="bit"><small>${bit}</small><span class="${set === 1 ? "set" : ""}">${set ?? "—"}</span></div>`; }).join("")}</div>`;
}
export function wireRows(root, selector, select) {
  const rows = [...root.querySelectorAll(selector)];
  rows.forEach((row,i) => {
    row.onclick = event => { if (!event.target.closest("button,a,input")) select(row); };
    row.onkeydown = event => {
      if (event.target !== row) return;
      const next = {ArrowDown:Math.min(rows.length-1,i+1),ArrowUp:Math.max(0,i-1),Home:0,End:rows.length-1}[event.key];
      if (next !== undefined) { event.preventDefault(); select(rows[next]); root.querySelectorAll(selector)[next]?.focus(); }
      else if (event.key === "Enter" || event.key === " ") { event.preventDefault(); select(row); }
    };
  });
}
let noticeTimer;
export function clearNotice() {
  clearTimeout(noticeTimer);
  document.getElementById("notice").classList.add("hidden");
}
export function notice(message, error = false) {
  const el = document.getElementById("notice"); el.textContent = message; el.className = "notice" + (error ? " error" : "");
  clearTimeout(noticeTimer); noticeTimer = setTimeout(() => el.classList.add("hidden"), error ? 12000 : 5500);
}
export function number(text, max, label = "Значение") {
  const s = String(text).trim();
  if (!/^(?:0x[\da-f]+|\d+)$/i.test(s)) throw new Error(label + ": введите целое число или 0xHEX");
  const n = /^0x/i.test(s) ? parseInt(s.slice(2), 16) : Number(s);
  if (!Number.isSafeInteger(n) || n < 0 || n > max) throw new Error(`${label}: допустимо от 0 до ${max}`);
  return n;
}
let dialogCompletion;
export async function ask({title, body, submit = "Продолжить", danger = false, run = async () => true}) {
  const dialog = document.getElementById("modal");
  if (dialog.open) return false;
  // close() hides the dialog immediately, but its close event is asynchronous.
  // Finish the old close handler before assigning handlers for a new dialog.
  await dialogCompletion;
  if (dialog.open) return false;
  const completion = new Promise(resolve => {
    const form = document.getElementById("modal-form");
    document.getElementById("modal-title").textContent = title;
    document.getElementById("modal-body").innerHTML = body;
    const error = document.getElementById("modal-error"), button = document.getElementById("modal-submit");
    error.textContent = ""; button.textContent = submit; button.className = danger ? "primary danger" : "primary"; button.disabled = false;
    let busy = false, accepted = false;
    const dismiss = () => { if (!busy) dialog.close(); };
    form.querySelectorAll("[data-dismiss]").forEach(b => { b.onclick = dismiss; b.disabled = false; });
    dialog.oncancel = event => { if (busy) event.preventDefault(); };
    dialog.onclose = () => { resolve(accepted); form.onsubmit = null; };
    form.onsubmit = async event => {
      event.preventDefault(); if (busy) return; busy = true; button.disabled = true;
      form.querySelectorAll("[data-dismiss]").forEach(b => { b.disabled = true; }); error.textContent = "";
      try { const result = await run(form); if (result !== false) { accepted = true; dialog.close(); } }
      catch (e) { error.textContent = e.message; }
      finally { busy = false; button.disabled = false; form.querySelectorAll("[data-dismiss]").forEach(b => { b.disabled = false; }); }
    };
    dialog.showModal();
  });
  dialogCompletion = completion;
  try { return await completion; }
  finally { if (dialogCompletion === completion) dialogCompletion = null; }
}
export const confirmAction = (title, message) => ask({title, body: `<p>${esc(message)}</p>`});
export async function action(button, work) {
  if (button?.disabled) return;
  if (button) button.disabled = true;
  try { await work(); } catch (e) { if (e.name !== "AbortError") notice(e.message, true); }
  finally { if (button?.isConnected) button.disabled = false; }
}
