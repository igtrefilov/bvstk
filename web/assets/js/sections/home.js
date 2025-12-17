import { apiGetFs, apiGetI2c, apiGetNet, apiGetRtos } from "../api.js";
import { byId, setDot, setText } from "../dom.js";
import { fmtBytes, fmtUptime } from "../format.js";

export async function loadHomeTiles() {
  await Promise.allSettled([loadNet(), loadRtos(), loadFs(), loadI2c()]);
}

async function loadNet() {
  try {
    const j = await apiGetNet();
    setText("net-ip", j.ip || "—");
    setText("net-netmask", j.netmask || "—");
    setText("net-gateway", j.gateway || "—");
    setText("net-mac", j.mac || "—");
    setText("net-mode", j.mode || "—");
    const ok = !!j.up && !!j.link_up && !!j.ip;
    setDot("net-dot", ok ? "ok" : "bad");
  } catch {
    setDot("net-dot", "bad");
  }
}

async function loadRtos() {
  try {
    const j = await apiGetRtos();
    setText("rtos-uptime", fmtUptime(j.uptime_ms));
    setText("rtos-heap-free", fmtBytes(j.heap_free));
    setText("rtos-heap-min", fmtBytes(j.heap_min_ever));
    setText("rtos-tick", j.tick_rate_hz ? `${j.tick_rate_hz} Hz` : "—");
    setDot("rtos-dot", "ok");
  } catch {
    setDot("rtos-dot", "bad");
  }
}

async function loadFs() {
  try {
    const j = await apiGetFs();
    const vols = Array.isArray(j.volumes) ? j.volumes : [];
    function fmtVol(name) {
      const v = vols.find((x) => x && x.name === name);
      if (!v) return "—";
      if (!v.ready) return "not ready";
      return `${fmtBytes(v.free_bytes)} free / ${fmtBytes(v.total_bytes)}`;
    }
    setText("fs-flash", fmtVol("flash"));
    setText("fs-sd", fmtVol("sd"));
    setDot("fs-dot", "ok");
  } catch {
    setDot("fs-dot", "bad");
  }
}

async function loadI2c() {
  try {
    const j = await apiGetI2c();
    const el = byId("i2c-list");
    const devices = Array.isArray(j.devices) ? j.devices : [];
    if (!j.ready) {
      setText("i2c-ready", "no");
      setText("i2c-count", "—");
      if (el) el.textContent = "config_store not ready";
      setDot("i2c-dot", "bad");
      return;
    }
    if (devices.length === 0) {
      setText("i2c-ready", "yes");
      setText("i2c-count", "0");
      if (el) el.textContent = "no devices";
      setDot("i2c-dot", "ok");
      return;
    }
    setText("i2c-ready", "yes");
    setText("i2c-count", String(devices.length));
    const rows = devices
      .map((d) => {
        const addr = typeof d.addr_7b === "number" ? `0x${d.addr_7b.toString(16).padStart(2, "0")}` : "—";
        const pol = d.policy || "—";
        const ap = d.autopoll_enabled ? "autopoll on" : "autopoll off";
        const wl = typeof d.whitelist_len === "number" ? d.whitelist_len : "—";
        const bl = typeof d.blacklist_len === "number" ? d.blacklist_len : "—";
        const boot = typeof d.settings_len === "number" ? d.settings_len : "—";
        return `${d.name || "—"} (${addr}) — ${pol}, ${ap}, wl:${wl}, bl:${bl}, boot:${boot}`;
      })
      .slice(0, 32);
    if (el) el.innerHTML = rows.map((r) => `<div style="padding:4px 0">${r}</div>`).join("");
    setDot("i2c-dot", "ok");
  } catch {
    setText("i2c-ready", "—");
    setText("i2c-count", "—");
    setDot("i2c-dot", "bad");
  }
}

