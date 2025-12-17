async function fetchJson(path) {
  const res = await fetch(path, { cache: "no-store" });
  if (!res.ok) throw new Error(`${path}: HTTP ${res.status}`);
  return await res.json();
}

async function putJson(path, payload) {
  const res = await fetch(path, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const text = await res.text();
  if (!res.ok) throw new Error(`PUT ${path}: HTTP ${res.status}: ${text.trim() || "failed"}`);
  try {
    return JSON.parse(text);
  } catch {
    return { ok: true };
  }
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

export function apiGetI2c(name) {
  if (name) return fetchJson(`/api/i2c?name=${encodeURIComponent(name)}`);
  return fetchJson("/api/i2c");
}

export async function apiPutI2c(payload) {
  return putJson("/api/i2c", payload);
}

export async function apiPutNet(payload) {
  return putJson("/api/net", payload);
}

export function apiDiagI2cRead(payload) {
  return putJson("/api/diag/i2c/read", payload);
}

export function apiDiagI2cWrite(payload) {
  return putJson("/api/diag/i2c/write", payload);
}

export function apiDiagSmiRead(payload) {
  return putJson("/api/diag/smi/read", payload);
}

export function apiDiagSmiWrite(payload) {
  return putJson("/api/diag/smi/write", payload);
}

export function apiDiagMemRead(payload) {
  return putJson("/api/diag/mem/read", payload);
}

export function apiDiagMemWrite(payload) {
  return putJson("/api/diag/mem/write", payload);
}

export function apiReboot(payload) {
  return putJson("/api/reboot", payload);
}
