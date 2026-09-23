// BVSTK device-view API 1. Runs only in the browser's isolated sandbox.
// All PHY interpretation comes from register_map.json; no bus operations here.
(() => {
  let root;
  const groups = [
    ["Режим линии",["autonegotiation","mdix","energy","phy_address"]],
    ["Питание и интерфейс",["power","energy_saving","isolation","loopback","far_loopback"]],
    ["Диагностические признаки",["remote_fault","jabber","symbol_errors","silicon_revision"]]
  ];
  const time = ms => {
    if (!Number.isFinite(ms)) return "—";
    const s = Math.floor(ms/1000), days = Math.floor(s/86400);
    return (days ? days+" д. " : "")+[Math.floor(s/3600)%24,Math.floor(s/60)%60,s%60].map(n=>String(n).padStart(2,"0")).join(":")+"."+String(ms%1000).padStart(3,"0");
  };
  const cell = id => `<section data-metric="${id}"><span data-title></span><strong data-value>Нет данных</strong><small data-time></small><p data-note></p></section>`;
  window.bvstkView = {
    apiVersion:1,
    mount(target) {
      root = target;
      const style = document.createElement("style");
      style.textContent = `
        h3{font-size:14px;margin:0 0 16px;font-weight:600}p{margin:8px 0 0;color:#697985;font-size:12px;line-height:1.5}
        .line{display:grid;grid-template-columns:1.4fr 1fr 1fr;gap:18px;padding:20px;border:1px solid #d7dfe4;border-left:4px solid #667783;background:#f6f8f9;margin-bottom:18px}
        .line.confirmed{border-left-color:#278142}.groups{display:grid;grid-template-columns:1fr 1.2fr 1.2fr;gap:18px}
        .group{border:1px solid #d7dfe4;padding:18px}section{min-width:0}section+section{margin-top:18px}.line section+section{margin-top:0}
        [data-title]{display:block;color:#586874;font-size:12px;margin-bottom:7px}[data-value]{font-size:15px;font-weight:600;display:block;overflow-wrap:anywhere}
        .line [data-value]{font-size:19px}small{display:block;color:#6e7c86;margin-top:6px;font-size:11px;font-variant-numeric:tabular-nums}
        .ok{color:#247840}.bad{color:#ac342b}.warn{color:#886213}.unknown{color:#68737c}.offline{color:#24313a}
        @media(max-width:800px){.groups{grid-template-columns:1fr}.line{grid-template-columns:1fr;gap:18px}.line [data-value]{font-size:18px}}
      `;
      document.head.append(style);
      root.innerHTML = `<div class="line">${cell("link")}${cell("speed")}${cell("duplex")}</div><div class="groups">${groups.map(([title,ids])=>`<div class="group"><h3>${title}</h3>${ids.map(cell).join("")}</div>`).join("")}</div>`;
    },
    update(snapshot) {
      for (const item of snapshot.summary) {
        const cell = root.querySelector(`[data-metric="${item.id}"]`);
        if (!cell) continue;
        cell.querySelector("[data-title]").textContent = item.title;
        cell.querySelector("[data-value]").textContent = item.label;
        cell.querySelector("[data-value]").className = item.tone;
        cell.querySelector("[data-time]").textContent = "Метка времени "+time(item.updated_ms);
        cell.querySelector("[data-note]").textContent = item.note || "";
      }
      root.querySelector(".line").classList.toggle("confirmed",!snapshot.stale && snapshot.device.presence === "responding" && snapshot.summary.find(item=>item.id==="link")?.tone === "ok");
    }
  };
})();
