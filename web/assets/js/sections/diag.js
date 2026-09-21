import {
  apiDiagI2cRead,
  apiDiagI2cWrite,
  apiDiagMemRead,
  apiDiagMemWrite,
  apiGetI2c,
} from "../api.js";
import { byId, setText } from "../dom.js";

function toInt(s) {
  const t = String(s || "").trim();
  if (!t) return null;
  const v = Number.parseInt(t, 0);
  if (!Number.isFinite(v) || !Number.isInteger(v)) return null;
  return v;
}

export async function initDiag() {
  await initDiagI2c();
  initDiagMem();
}

async function initDiagI2c() {
  const sel = byId("diag-i2c-dev");
  if (!sel) return;

  try {
    const j = await apiGetI2c();
    const devs = Array.isArray(j.devices) ? j.devices : [];
    if (!j.ready || devs.length === 0) {
      sel.innerHTML = `<option value="">(no devices)</option>`;
      return;
    }
    sel.innerHTML = devs
      .map((d) => {
        const name = d && d.name ? d.name : "";
        const addr = typeof d.addr_7b === "number" ? `0x${d.addr_7b.toString(16).padStart(2, "0")}` : "—";
        return `<option value="${name}">${name} (${addr})</option>`;
      })
      .join("");
    sel.value = devs[0].name || "";
  } catch {
    sel.innerHTML = `<option value="">(load failed)</option>`;
  }

  byId("diag-i2c-read")?.addEventListener("click", async () => {
    const name = sel.value;
    const reg = toInt(byId("diag-i2c-reg")?.value);
    if (!name) return void setText("diag-i2c-out", "select device");
    if (reg === null || reg < 0 || reg > 255) return void setText("diag-i2c-out", "bad reg");
    setText("diag-i2c-out", "reading...");
    try {
      const r = await apiDiagI2cRead({ name, reg });
      setText("diag-i2c-out", `OK ${r.name || name} reg ${reg} = ${r.val}`);
    } catch (e) {
      setText("diag-i2c-out", `ERR (${e?.message || "failed"})`);
    }
  });

  byId("diag-i2c-write")?.addEventListener("click", async () => {
    const name = sel.value;
    const reg = toInt(byId("diag-i2c-reg")?.value);
    const val = toInt(byId("diag-i2c-val")?.value);
    if (!name) return void setText("diag-i2c-out", "select device");
    if (reg === null || reg < 0 || reg > 255) return void setText("diag-i2c-out", "bad reg");
    if (val === null || val < 0 || val > 255) return void setText("diag-i2c-out", "bad val");
    setText("diag-i2c-out", "writing...");
    try {
      await apiDiagI2cWrite({ name, reg, val });
      setText("diag-i2c-out", "OK");
    } catch (e) {
      setText("diag-i2c-out", `ERR (${e?.message || "failed"})`);
    }
  });
}

function initDiagMem() {
  byId("diag-mem-read")?.addEventListener("click", async () => {
    const addr = toInt(byId("diag-mem-addr")?.value);
    if (addr === null || addr < 0) return void setText("diag-mem-out", "bad addr");
    setText("diag-mem-out", "reading...");
    try {
      const r = await apiDiagMemRead({ addr });
      const v = Number(r.val);
      const hex = r.width === 8 ? `0x${v.toString(16).padStart(2, "0")}` : `0x${v.toString(16).padStart(8, "0")}`;
      setText("diag-mem-out", `OK ${hex} (${v}) width=${r.width}`);
    } catch (e) {
      setText("diag-mem-out", `ERR (${e?.message || "failed"})`);
    }
  });

  byId("diag-mem-write")?.addEventListener("click", async () => {
    const addr = toInt(byId("diag-mem-addr")?.value);
    const val = toInt(byId("diag-mem-val")?.value);
    const confirm = !!byId("diag-mem-confirm")?.checked;
    if (addr === null || addr < 0) return void setText("diag-mem-out", "bad addr");
    if (val === null) return void setText("diag-mem-out", "bad val");
    if (!confirm) return void setText("diag-mem-out", "confirm required");
    setText("diag-mem-out", "writing...");
    try {
      const r = await apiDiagMemWrite({ addr, val, confirm: true });
      setText("diag-mem-out", `OK width=${r.width}`);
    } catch (e) {
      setText("diag-mem-out", `ERR (${e?.message || "failed"})`);
    }
  });
}
