import * as api from "./api.js";

// Link monitoring is independent of page refreshes, drafts and dialogs.
// Keep the embedded server's single-connection queue free for actual work.
export function startConnectionMonitor(element, {interval = 1000, timeout = 2000} = {}) {
  let timer, probe, state, lastReply = 0, stopped = false;
  function render(connected) {
    const next = connected && navigator.onLine !== false;
    if (state === next) return;
    state = next;
    element.dataset.state = next ? "online" : "offline";
    element.title = next ? "Есть связь с устройством bvstk" : "Нет связи с устройством bvstk";
    element.innerHTML = `<span class="dot ${next ? "ok" : "offline"}" aria-hidden="true"></span><span>${next ? "Онлайн" : "Оффлайн"}</span>`;
  }
  function schedule(delay = interval) {
    clearTimeout(timer);
    if (!stopped) timer = setTimeout(check, delay);
  }
  async function check() {
    if (stopped || probe) return;
    if (navigator.onLine === false) { render(false); schedule(); return; }
    if (document.hidden || api.pending || Date.now() < api.downloadPauseUntil) { schedule(); return; }
    const age = Date.now() - lastReply;
    if (lastReply && age < interval) { schedule(interval - age); return; }
    probe = new AbortController();
    try { await api.request("/api/version", {signal:probe.signal, timeout}); }
    catch { /* Transport events update the indicator; HTTP errors still prove connectivity. */ }
    finally { probe = null; schedule(); }
  }
  function response(event) {
    if (event.detail.ok) lastReply = event.detail.at;
    else lastReply = 0;
    render(event.detail.ok);
  }
  function offline() {
    lastReply = 0; render(false);
    // A browser network loss invalidates the external download's polling pause.
    api.pausePolling(0);
    probe?.abort();
  }
  function retry() {
    lastReply = 0;
    if (!probe) schedule(0);
  }
  function visible() { if (!document.hidden) retry(); }
  window.addEventListener("bvstk-connection", response);
  window.addEventListener("offline", offline);
  window.addEventListener("online", retry);
  document.addEventListener("visibilitychange", visible);
  render(false); schedule();
  return () => {
    stopped = true; clearTimeout(timer); probe?.abort();
    window.removeEventListener("bvstk-connection", response);
    window.removeEventListener("offline", offline);
    window.removeEventListener("online", retry);
    document.removeEventListener("visibilitychange", visible);
  };
}
