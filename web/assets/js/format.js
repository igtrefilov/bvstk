export function fmtBytes(n) {
  if (typeof n !== "number" || !isFinite(n)) return "—";
  const units = ["B", "KiB", "MiB", "GiB"];
  let v = n;
  let u = 0;
  while (v >= 1024 && u < units.length - 1) {
    v /= 1024;
    u++;
  }
  return `${v.toFixed(u === 0 ? 0 : 1)} ${units[u]}`;
}

export function fmtUptime(ms) {
  if (typeof ms !== "number" || !isFinite(ms)) return "—";
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const ss = s % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${ss}s`;
}

export function isIpv4(s) {
  if (typeof s !== "string") return false;
  const m = s.trim().match(/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/);
  if (!m) return false;
  for (let i = 1; i <= 4; i++) {
    const v = Number(m[i]);
    if (!Number.isInteger(v) || v < 0 || v > 255) return false;
  }
  return true;
}

export function isMac(s) {
  if (typeof s !== "string") return false;
  return /^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$/.test(s.trim());
}

