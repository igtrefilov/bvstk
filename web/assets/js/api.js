// Single request queue: the embedded HTTP server serves one connection at a time.
let tail = Promise.resolve();
export let pending = 0;
export let downloadPauseUntil = 0;
export function pausePolling(ms = 120000) { downloadPauseUntil = Date.now() + ms; }
export function url(path, params = {}) {
  const q = new URLSearchParams();
  for (const [key, value] of Object.entries(params)) if (value !== undefined && value !== null) q.set(key, value);
  return path + (q.size ? "?" + q : "");
}
function online(ok) { window.dispatchEvent(new CustomEvent("bvstk-connection", { detail: {ok, at: Date.now()} })); }
export function queue(work) {
  pending++;
  const run = tail.catch(() => {}).then(work);
  tail = run.catch(() => {});
  return run.finally(() => { pending--; });
}
export async function request(path, {method = "GET", body, signal, timeout = 20000, raw = false} = {}) {
  return queue(async () => {
    if (signal?.aborted) throw new DOMException("Aborted", "AbortError");
    const controller = new AbortController();
    const cancel = () => controller.abort();
    signal?.addEventListener("abort", cancel, {once: true});
    const timer = setTimeout(cancel, timeout);
    let transportComplete = false;
    try {
      const response = await fetch(path, {method, body: body === undefined ? undefined : JSON.stringify(body),
        headers: body === undefined ? {} : {"Content-Type": "application/json"}, cache: "no-store", signal: controller.signal});
      const text = await response.text();
      transportComplete = true;
      online(true);
      if (raw && response.ok) return text;
      let data;
      try { data = JSON.parse(text); } catch { data = {error: text.trim() || "Пустой ответ сервера"}; }
      if (!response.ok || data.ok === false) {
        const error = new Error(data.error || "Операция не выполнена");
        error.status = response.status;
        throw error;
      }
      if (data.error !== undefined && !data.ok) throw new Error(String(data.error));
      return data;
    } catch (error) {
      if (!transportComplete && !signal?.aborted) online(false);
      if (error.name === "AbortError" && !signal?.aborted) throw new Error("Время ожидания истекло. Проверьте результат перед повтором операции.");
      throw error;
    } finally { clearTimeout(timer); signal?.removeEventListener("abort", cancel); }
  });
}
export const get = (path, signal) => request(path, {signal});
export const busList = async (bus, signal) => {
  const data = await get(url("/api/ui/bus", {bus}), signal);
  window.dispatchEvent(new CustomEvent("bvstk-devices", {detail:{bus,data}}));
  return data;
};
export const busGet = (bus, name, signal) => get(url("/api/ui/bus", {bus, name}), signal);
export const busAction = (bus, name, action, params = {}, body) =>
  request(url("/api/ui/bus", {bus, name, action, ...params}), {method: "PUT", body: body ?? {}});
export const volumes = async signal => {
  const data = await get("/api/ui/volumes", signal);
  window.dispatchEvent(new CustomEvent("bvstk-volumes", {detail:data}));
  return data;
};
export const fsList = (volume, path, offset, signal) => get(url("/api/ui/files", {volume, path, offset, limit: 64}), signal);
export const fsStat = (volume, path) => get(url("/api/ui/files", {volume, path, action: "stat"}));
export const fsAction = (volume, path, action, extra = {}) => request(url("/api/ui/files", {volume, path, action, confirm: true, ...extra}), {method: "PUT", body: {}, timeout: action === "copy" ? 1800000 : 60000});
export const fileUrl = (volume, path, max_bytes) => url("/api/ui/file", {volume, path, max_bytes});
export function upload(volume, path, file, overwrite, onProgress) {
  return queue(() => new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("PUT", url("/api/ui/file", {volume, path, overwrite, confirm: true}));
    xhr.timeout = Math.max(30000, 20000 + file.size * 1000 / 4096);
    xhr.upload.onprogress = event => { if (event.lengthComputable) onProgress?.(event.loaded, event.total); };
    xhr.onerror = () => { online(false); reject(new Error("Связь потеряна. Файл может быть записан частично.")); };
    xhr.ontimeout = () => { online(false); reject(new Error("Истекло время передачи. Проверьте файл перед повторной загрузкой.")); };
    xhr.onload = () => {
      online(true);
      let data;
      try { data = JSON.parse(xhr.responseText); } catch { data = {error: xhr.responseText}; }
      if (xhr.status < 200 || xhr.status >= 300 || data.ok !== true || data.bytes !== file.size)
        reject(new Error(data.error || "Сервер не подтвердил полную запись файла"));
      else resolve(data);
    };
    xhr.send(file);
  }));
}
