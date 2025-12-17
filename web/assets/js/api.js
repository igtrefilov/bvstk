async function fetchJson(path) {
  const res = await fetch(path, { cache: "no-store" });
  if (!res.ok) throw new Error(`${path}: HTTP ${res.status}`);
  return await res.json();
}

export function apiGetNet() {
  return fetchJson("/api/net");
}

export function apiGetRtos() {
  return fetchJson("/api/rtos");
}

export function apiGetFs() {
  return fetchJson("/api/fs");
}

export function apiGetI2c() {
  return fetchJson("/api/i2c");
}

export async function apiPutI2c(payload) {
  const res = await fetch("/api/i2c", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const text = await res.text();
  if (!res.ok) throw new Error(`PUT /api/i2c: HTTP ${res.status}: ${text.trim() || "failed"}`);
  try {
    return JSON.parse(text);
  } catch {
    return { ok: true };
  }
}

export async function apiPutNet(payload) {
  const res = await fetch("/api/net", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const text = await res.text();
  if (!res.ok) throw new Error(`PUT /api/net: HTTP ${res.status}: ${text.trim() || "failed"}`);
  try {
    return JSON.parse(text);
  } catch {
    return { ok: true };
  }
}
