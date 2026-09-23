// Optional device UI runs in an opaque-origin sandbox, never in the main page.
// The message protocol contains snapshots only, not API/operation capabilities.
const base64 = source => btoa(String.fromCharCode(...new TextEncoder().encode(source)));
export function deviceView(root, source, {onReady, onError}) {
  const channel = [...crypto.getRandomValues(new Uint32Array(4))].map(n => n.toString(16)).join("");
  const iframe = document.createElement("iframe");
  iframe.className = "device-indicators-frame";
  iframe.title = "Показатели устройства";
  iframe.setAttribute("sandbox", "allow-scripts");
  iframe.setAttribute("referrerpolicy", "no-referrer");
  iframe.hidden = true;
  let last, ready = false, stopped = false;
  const fail = message => {
    if (stopped) return;
    stopped = true; clearTimeout(timer); window.removeEventListener("message", receive);
    iframe.remove(); onError(message);
  };
  const receive = event => {
    if (event.source !== iframe.contentWindow || event.data?.channel !== channel || stopped) return;
    if (event.data.type === "ready") {
      ready = true; clearTimeout(timer); iframe.hidden = false; onReady();
      if (last) iframe.contentWindow.postMessage({channel,type:"snapshot",snapshot:last}, "*");
    } else if (event.data.type === "height" && Number.isFinite(event.data.height)) {
      const height = Math.max(80,Math.min(3000,Math.ceil(event.data.height)));
      if (iframe.style.height !== height + "px") iframe.style.height = height + "px";
    } else if (event.data.type === "error") fail("Ошибка формы устройства: " + String(event.data.message || "неизвестная ошибка").slice(0,200));
  };
  const timer = setTimeout(() => fail("Форма устройства не запустилась. Показана стандартная расшифровка."),5000);
  window.addEventListener("message", receive);
  const bridge = `
    const channel=${JSON.stringify(channel)};
    const send=(type,extra={})=>parent.postMessage({channel,type,...extra},'*');
    const report=error=>send('error',{message:String(error?.message||error).slice(0,200)});
    let started=false;
    addEventListener('error',e=>report(e.error||e.message));
    addEventListener('unhandledrejection',e=>report(e.reason));
    addEventListener('message',e=>{
      if(e.source!==parent||e.data?.channel!==channel||e.data.type!=='snapshot'||!started)return;
      try { window.bvstkView.update(e.data.snapshot); } catch(error) { report(error); }
    });
    addEventListener('load',()=>{
      try {
        const view=window.bvstkView;
        if(view?.apiVersion!==1||typeof view.mount!=='function'||typeof view.update!=='function')throw Error('Неверный API формы');
        view.mount(document.getElementById('device-root'));
        started=true;
        new ResizeObserver(()=>send('height',{height:document.body.scrollHeight})).observe(document.body);
        send('ready');
      } catch(error) { report(error); }
    });`;
  // A nonce allows these two scripts only. No allow-same-origin, network,
  // forms, popups, external images, nested frames or main-page DOM access.
  iframe.srcdoc = `<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'nonce-${channel}'; style-src 'unsafe-inline'; connect-src 'none'; form-action 'none'; base-uri 'none'"><style>html,body{margin:0;padding:0;background:#fff;color:#24313a;font:14px Arial,sans-serif}*{box-sizing:border-box}#device-root{display:flow-root}</style></head><body><div id="device-root"></div><script nonce="${channel}" src="data:text/javascript;base64,${base64(bridge)}"></script><script nonce="${channel}" src="data:text/javascript;base64,${base64(source)}"></script></body></html>`;
  root.append(iframe);
  return {
    update(snapshot) { last = snapshot; if (ready && !stopped) iframe.contentWindow.postMessage({channel,type:"snapshot",snapshot},"*"); },
    dispose() { stopped = true; clearTimeout(timer); window.removeEventListener("message",receive); iframe.remove(); }
  };
}
