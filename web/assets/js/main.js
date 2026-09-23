import * as api from "./api.js";
import {icon, esc, notice, ask, stale, clearNotice} from "./ui.js";
import {overview, sdPage} from "./overview.js";
import {busPage} from "./buses.js";
import {filesPage} from "./files.js";
import {systemPage} from "./system.js";
import {startConnectionMonitor} from "./connection.js";

startConnectionMonitor(document.getElementById("connection"));

document.querySelectorAll("[data-icon]").forEach(el => { el.innerHTML = icon(el.dataset.icon); });
document.querySelector(".brand-waves").innerHTML = Array.from({length:12},(_,i) =>
  `<path d="M1 ${2+i*1.6}C32 ${20+i*.55} 68 ${29-i*.3} 111 ${6+i*1.5}M1 ${22-i*.5}C43 ${29-i*.3} 79 ${16+i*.4} 111 ${2+i*1.4}"/>`).join("");
const menu = document.getElementById("menu-toggle"), sidebar = document.getElementById("sidebar");
document.querySelector(".skip-link").onclick = event => { event.preventDefault(); document.getElementById("content").focus(); };
menu.onclick = () => { const open = sidebar.classList.toggle("open"); menu.setAttribute("aria-expanded", String(open)); };
document.addEventListener("keydown", event => { if (event.key === "Escape") { sidebar.classList.remove("open"); menu.setAttribute("aria-expanded","false"); } });
let view, controller, hash = "", navigating = false, refreshBusy = false, pollTimer;
let context = {section:"overview"}, knownVolumes = false;
const knownBuses = new Set();
function selectNavigation() {
  sidebar.querySelectorAll("a").forEach(el => {
    let active = false;
    if (el.dataset.deviceName) active = el.dataset.deviceBus === context.section && el.dataset.deviceName === context.name;
    else if (el.dataset.volumeNav) active = context.section === "files" && el.dataset.volumeNav === context.volume;
    else if (el.dataset.systemNav) active = context.section === "system" && el.dataset.systemNav === context.tab;
    else if (el.dataset.nav) active = el.dataset.nav === context.section &&
      !(["i2c","mdio"].includes(context.section) && context.name) &&
      !(context.section === "files" && context.volume) &&
      !(context.section === "system" && context.tab);
    el.classList.toggle("active",active);
    if (active) {
      el.setAttribute("aria-current","page");
      const branch = el.closest("details"); if (branch) branch.open = true;
    } else el.removeAttribute("aria-current");
  });
  document.getElementById("object-status").textContent = context.object ||
    context.name || (context.volume ? context.volume + ":/" :
      {overview:"Состояние",system:"Система","sd-pl":"SD-PL",files:"Файлы",i2c:"I²C",mdio:"MDIO"}[context.section] || context.section);
}
window.addEventListener("bvstk-devices", event => {
  const {bus,data} = event.detail; knownBuses.add(bus);
  const tree = sidebar.querySelector(`[data-device-tree="${bus}"]`);
  if (!tree) return;
  tree.innerHTML = data.devices.map(d => `<a href="#${bus}?name=${encodeURIComponent(d.name)}" data-device-bus="${bus}" data-device-name="${esc(d.name)}">${icon("chip")}${esc(d.name)}</a>`).join("");
  selectNavigation();
});
window.addEventListener("bvstk-volumes", event => {
  knownVolumes = true;
  for (const v of event.detail.volumes) {
    const el = sidebar.querySelector(`[data-volume-nav="${v.name}"]`);
    if (el) el.title = v.ready ? "Смонтирован" : "Том недоступен";
  }
});
async function loadNavigation() {
  for (const bus of ["i2c","mdio"]) if (!knownBuses.has(bus)) { try { await api.busList(bus); } catch {} }
  if (!knownVolumes) { try { await api.volumes(); } catch {} }
}
async function runRefresh(refresh) {
  if (refreshBusy) return;
  refreshBusy = true;
  const root = document.getElementById("content").firstElementChild;
  clearNotice();
  try { await refresh(); } catch (e) { if (e.name !== "AbortError" && root?.isConnected) { stale(root, e); notice(e.message,true); } }
  finally { refreshBusy = false; }
}
async function route() {
  if (navigating) return;
  navigating = true;
  try {
    const targetHash = location.hash || "#overview";
    if (view?.busy?.()) { history.replaceState(null,"",hash); notice("Дождитесь завершения операции"); return; }
    if (view?.dirty?.() && !await ask({title:"Покинуть страницу?",body:"<p>Черновик браузера будет потерян. Уже применённые изменения RAM остаются активны.</p>"})) {
      history.replaceState(null,"",hash); return;
    }
    controller?.abort(); clearTimeout(pollTimer); controller = new AbortController(); view = null;
    document.getElementById("notice").classList.add("hidden");
    hash = targetHash;
    const [name,query = ""] = targetHash.slice(1).split("?"), section = name || "overview", params = new URLSearchParams(query);
    context = {section,name:params.get("name"),volume:section === "files" ? params.get("volume") || "sd-pl" : null,tab:section === "system" ? params.get("tab") || "network" : null};
    selectNavigation();
    sidebar.classList.remove("open"); menu.setAttribute("aria-expanded","false");
    const root = document.createElement("div"); root.innerHTML = '<div class="loading">Загрузка данных…</div>';
    document.getElementById("content").replaceChildren(root);
    const signal = controller.signal;
    const ctx = {signal,runRefresh,setContext(details) {
      if (signal.aborted) return;
      context = {...context,...details}; selectNavigation();
      // Update deep links without rerouting or discarding a draft.
      for (const key of ["name","tab","volume","path"]) if (details[key] != null) params.set(key,details[key]);
      if (location.hash === hash || (!location.hash && hash === "#overview")) {
        hash = "#" + section + (params.size ? "?" + params : "");
        history.replaceState(null,"",hash);
      }
    }};
    const factory = {overview,"sd-pl":sdPage,files:filesPage,system:systemPage,i2c:(...args)=>busPage(...args,"i2c"),mdio:(...args)=>busPage(...args,"mdio")}[section];
    if (!factory) throw new Error("Неизвестный раздел");
    view = await factory(root,params,ctx);
  } catch(e) {
    if (e.name !== "AbortError") {
      document.getElementById("content").innerHTML = '<article class="card"><h1>Не удалось загрузить данные</h1><p class="error-text">'+esc(e.message)+'</p><p>Проверьте связь и версию PS-прошивки. Новый интерфейс требует API /api/ui/.</p><button id="retry">Повторить</button></article>';
      document.getElementById("retry").onclick = route;
    }
  } finally {
    navigating = false; schedulePoll();
    // A second navigation during loading is not lost.
    if (location.hash && location.hash !== hash) route();
  }
}
function schedulePoll() {
  clearTimeout(pollTimer);
  pollTimer = setTimeout(async () => {
    if (!document.hidden && !navigating && !api.pending && !refreshBusy && !document.getElementById("modal").open && Date.now() >= api.downloadPauseUntil && !view?.dirty?.()) {
      if (view?.poll) await runRefresh(() => view.refresh(true));
    }
    schedulePoll();
  },15000);
}
window.addEventListener("hashchange",route);
window.addEventListener("beforeunload",event => { if (view?.dirty?.()) { event.preventDefault(); event.returnValue = ""; } });
document.addEventListener("visibilitychange",() => { if (!document.hidden) schedulePoll(); });
route().then(loadNavigation);
