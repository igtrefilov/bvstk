import { apiGetI2c, apiPutI2c } from "../api.js";
import { byId, setText } from "../dom.js";

function parseRegsList(text) {
  const s = String(text || "").trim();
  if (!s) return [];
  const parts = s.split(/[\s,]+/).filter(Boolean);
  const regs = [];
  for (const p of parts) {
    const v = Number(p);
    if (!Number.isInteger(v) || v < 0 || v > 255) return null;
    regs.push(v);
  }
  return regs;
}

function regsToString(regs) {
  if (!Array.isArray(regs) || regs.length === 0) return "";
  return regs.join(", ");
}

function parseRulesText(text) {
  const lines = String(text || "")
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter((l) => l.length > 0);
  const out = [];
  for (const line of lines) {
    const m = line.split(/[\s=,]+/).filter(Boolean);
    if (m.length < 2) return null;
    const reg = Number.parseInt(m[0], 0);
    const val = Number.parseInt(m[1], 0);
    if (!Number.isInteger(reg) || !Number.isInteger(val) || reg < 0 || reg > 255 || val < 0 || val > 255) return null;
    out.push({ reg, val });
  }
  return out;
}

function rulesToText(rules) {
  if (!Array.isArray(rules) || rules.length === 0) return "";
  return rules.map((r) => `${r.reg}=${r.val}`).join("\n");
}

export function initI2cSettings(onSaved) {
  const form = byId("i2c-form");
  const sel = byId("i2c-dev");
  if (!form || !sel) return;

  let devices = [];

  async function refreshDevices() {
    try {
      const j = await apiGetI2c();
      if (!j || !j.ready) {
        devices = [];
        sel.innerHTML = `<option value="">(not ready)</option>`;
        setText("i2c-status", "config_store not ready");
        return;
      }
      devices = Array.isArray(j.devices) ? j.devices : [];
      if (devices.length === 0) {
        sel.innerHTML = `<option value="">(no devices)</option>`;
        setText("i2c-status", "no devices");
        return;
      }
      sel.innerHTML = devices
        .map((d) => {
          const name = d && d.name ? d.name : "";
          const addr = typeof d.addr_7b === "number" ? `0x${d.addr_7b.toString(16).padStart(2, "0")}` : "—";
          return `<option value="${name}">${name} (${addr})</option>`;
        })
        .join("");
      sel.value = devices[0].name || "";
      await fillFromSelected();
      setText("i2c-status", "loaded");
    } catch (e) {
      setText("i2c-status", `failed to load (${e && e.message ? e.message : "error"})`);
    }
  }

  function currentDevice() {
    const name = sel.value;
    return devices.find((d) => d && d.name === name) || null;
  }

  async function fillFromSelected() {
    const meta = currentDevice();
    if (!meta || !meta.name) return;
    try {
      const j = await apiGetI2c(meta.name);
      const d = j && j.device ? j.device : null;
      if (!d) return;
      byId("i2c-policy").value = d.policy === "blacklist" ? "blacklist" : "whitelist";
      byId("i2c-autopoll-enabled").checked = !!d.autopoll_enabled;
      byId("i2c-autopoll-regs").value = regsToString(d.autopoll_regs || []);
      byId("i2c-whitelist").value = rulesToText(d.whitelist || []);
      byId("i2c-blacklist").value = rulesToText(d.blacklist || []);
      byId("i2c-reg-delay").value =
        typeof d.autopoll_reg_delay_ms === "number" ? String(d.autopoll_reg_delay_ms) : "";
      byId("i2c-cycle-delay").value =
        typeof d.autopoll_cycle_delay_ms === "number" ? String(d.autopoll_cycle_delay_ms) : "";
    } catch (_) {
      // ignore; keep whatever was there
    }
  }

  async function save() {
    const name = sel.value;
    if (!name) return void setText("i2c-status", "select device");

    const policy = byId("i2c-policy").value;
    const autopoll_enabled = !!byId("i2c-autopoll-enabled").checked;
    const regs = parseRegsList(byId("i2c-autopoll-regs").value);
    if (regs === null) return void setText("i2c-status", "bad regs list");

    const whitelist = parseRulesText(byId("i2c-whitelist").value);
    if (whitelist === null) return void setText("i2c-status", "bad whitelist");
    const blacklist = parseRulesText(byId("i2c-blacklist").value);
    if (blacklist === null) return void setText("i2c-status", "bad blacklist");

    const regDelay = byId("i2c-reg-delay").value.trim();
    const cycleDelay = byId("i2c-cycle-delay").value.trim();
    const autopoll_reg_delay_ms = regDelay ? Number(regDelay) : 0;
    const autopoll_cycle_delay_ms = cycleDelay ? Number(cycleDelay) : 0;
    if (!Number.isInteger(autopoll_reg_delay_ms) || autopoll_reg_delay_ms < 0) {
      return void setText("i2c-status", "bad reg delay");
    }
    if (!Number.isInteger(autopoll_cycle_delay_ms) || autopoll_cycle_delay_ms < 0) {
      return void setText("i2c-status", "bad cycle delay");
    }

    setText("i2c-status", "saving...");
    try {
      await apiPutI2c({
        name,
        policy,
        autopoll_enabled,
        autopoll_regs: regs,
        whitelist,
        blacklist,
        autopoll_reg_delay_ms,
        autopoll_cycle_delay_ms,
      });
      setText("i2c-status", "saved");
      await refreshDevices();
      if (typeof onSaved === "function") onSaved();
    } catch (e) {
      setText("i2c-status", `save failed (${e && e.message ? e.message : "error"})`);
    }
  }

  sel.addEventListener("change", async () => {
    await fillFromSelected();
    setText("i2c-status", "ready");
  });

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    await save();
  });

  refreshDevices();
}
