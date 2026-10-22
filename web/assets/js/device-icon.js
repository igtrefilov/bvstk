import {icon} from "./ui.js";
import {readBundleAsset} from "./device-assets.js";

const base64 = source => btoa(String.fromCharCode(...new TextEncoder().encode(source)));

// Device code stays inside an opaque-origin frame; only snapshots cross it.
function iconFrame(root, source, {onReady, onError}) {
  const channel = [...crypto.getRandomValues(new Uint32Array(4))].map(n => n.toString(16)).join("");
  const frame = document.createElement("iframe");
  frame.className = "device-icon-frame";
  frame.title = "Пиктограмма устройства";
  frame.tabIndex = -1;
  frame.setAttribute("aria-hidden","true");
  frame.setAttribute("sandbox","allow-scripts");
  frame.setAttribute("referrerpolicy","no-referrer");
  frame.hidden = true;
  let stopped = false, ready = false, last;
  const dispose = () => {
    stopped = true; clearTimeout(timer);
    window.removeEventListener("message",receive); frame.remove();
  };
  const fail = message => { if (!stopped) { dispose(); onError(message); } };
  const receive = event => {
    if (stopped || event.source !== frame.contentWindow || event.data?.channel !== channel) return;
    if (event.data.type === "ready" && !ready) {
      ready = true; clearTimeout(timer); frame.hidden = false; onReady();
      if (last) frame.contentWindow.postMessage({channel,type:"snapshot",snapshot:last},"*");
    } else if (event.data.type === "error") fail("Ошибка пиктограммы: " + String(event.data.message || "ошибка скрипта").slice(0,200));
  };
  const timer = setTimeout(() => fail("Скрипт пиктограммы не запустился"),5000);
  window.addEventListener("message",receive);
  const bridge = `
    const channel=${JSON.stringify(channel)};
    const send=(type,extra={})=>parent.postMessage({channel,type,...extra},'*');
    const report=error=>send('error',{message:String(error?.message||error).slice(0,200)});
    let started=false;
    addEventListener('error',e=>{e.preventDefault();report(e.error||e.message);});
    addEventListener('unhandledrejection',e=>{e.preventDefault();report(e.reason);});
    addEventListener('message',e=>{
      if(e.source!==parent||e.data?.channel!==channel||e.data.type!=='snapshot'||!started)return;
      try { window.bvstkIcon.update(e.data.snapshot); } catch(error) { report(error); }
    });
    addEventListener('load',()=>{
      try {
        const icon=window.bvstkIcon;
        if(icon?.apiVersion!==1||typeof icon.mount!=='function'||typeof icon.update!=='function')throw Error('Неверный API пиктограммы');
        icon.mount(document.getElementById('icon-root'));
        started=true; send('ready');
      } catch(error) { report(error); }
    });`;
  frame.srcdoc = `<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'nonce-${channel}'; style-src 'unsafe-inline'; connect-src 'none'; form-action 'none'; base-uri 'none'"><style>html,body,#icon-root{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:transparent}#icon-root{color:#7d8d98}svg{display:block;width:100%;height:100%;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}</style></head><body><div id="icon-root"></div><script nonce="${channel}" src="data:text/javascript;base64,${base64(bridge)}"></script><script nonce="${channel}" src="data:text/javascript;base64,${base64(source)}"></script></body></html>`;
  root.append(frame);
  return {
    update(snapshot) { last = snapshot; if (ready && !stopped) frame.contentWindow.postMessage({channel,type:"snapshot",snapshot},"*"); },
    dispose
  };
}

export function deviceIconWorkspace(root, {bus,detail,signal}) {
  const controller = new AbortController();
  let renderer, disposed = false, stale = false;
  root.innerHTML = icon("chip",{className:"device-type-icon"});
  const fallback = root.firstElementChild;
  root.dataset.iconState = detail.icon ? "loading" : "fallback";
  root.removeAttribute("title");
  const fail = message => {
    fallback.removeAttribute("hidden"); root.dataset.iconState = "fallback";
    root.title = "Показана стандартная пиктограмма. " + message;
  };
  function paint() {
    const metadata = detail.config?.device || {};
    root.setAttribute("aria-label","Пиктограмма " + (metadata.model || detail.config?.name || "устройства"));
    renderer?.update({api_version:1,
      device:{...metadata,id:detail.name || detail.config?.name,bus,presence:detail.presence || "unknown",address_spaces:detail.address_spaces || []},
      stale,uptime_ms:Number.isFinite(detail.uptime_ms) ? detail.uptime_ms : null,
      cache:detail.cache || [],appearance:{size:48,color:"#7d8d98"}});
  }
  const dispose = () => {
    if (disposed) return;
    disposed = true; controller.abort(); renderer?.dispose();
    signal.removeEventListener("abort",dispose);
  };
  signal.addEventListener("abort",dispose,{once:true});
  if (signal.aborted) dispose();
  paint();
  async function start() {
    if (disposed || !detail.icon) return;
    try {
      if (typeof detail.icon !== "string" || !/^[a-zA-Z0-9_-]+\.js$/.test(detail.icon)) throw Error("Неверное имя файла пиктограммы");
      const source = await readBundleAsset(bus,detail.file_name,detail.icon,controller.signal,16384);
      if (disposed) return;
      renderer = iconFrame(root,source,{
        onReady:() => { fallback.setAttribute("hidden",""); root.dataset.iconState = "ready"; },
        onError:fail
      });
      paint();
    } catch (error) { if (!disposed) fail(error.message); }
  }
  void start();
  return {update(next,unavailable = false) { if (!disposed) { detail = next; stale = unavailable; paint(); } },dispose};
}
