import { apiGetNet, apiPutNet } from "../api.js";
import { byId, setText } from "../dom.js";
import { isIpv4, isMac } from "../format.js";

export function initSettings() {
  const form = byId("net-form");
  if (!form) return;

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    await saveNetConfig();
  });

  loadNetConfigToForm();
}

async function loadNetConfigToForm() {
  try {
    const j = await apiGetNet();
    const ip = (j.ip || "").trim();
    const nm = (j.netmask || "").trim();
    const gw = (j.gateway || "").trim();
    const mac = (j.mac || "").trim();
    if (ip) byId("cfg-ip").value = ip;
    if (nm) byId("cfg-netmask").value = nm;
    if (gw) byId("cfg-gateway").value = gw;
    if (mac) byId("cfg-mac").value = mac;
    setText("cfg-status", "loaded");
  } catch {
    setText("cfg-status", "failed to load");
  }
}

async function saveNetConfig() {
  const ip = byId("cfg-ip").value.trim();
  const netmask = byId("cfg-netmask").value.trim();
  const gateway = byId("cfg-gateway").value.trim();
  const mac = byId("cfg-mac").value.trim();
  const apply = !!byId("cfg-apply").checked;

  if (!isIpv4(ip)) return void setText("cfg-status", "bad IP");
  if (!isIpv4(netmask)) return void setText("cfg-status", "bad netmask");
  if (!isIpv4(gateway)) return void setText("cfg-status", "bad gateway");
  if (!isMac(mac)) return void setText("cfg-status", "bad MAC");

  setText("cfg-status", "saving...");
  try {
    await apiPutNet({ ip, netmask, gateway, mac, apply });
    setText("cfg-status", apply ? "saved (applied)" : "saved");
  } catch (e) {
    setText("cfg-status", `save failed (${e && e.message ? e.message : "error"})`);
  }
}

