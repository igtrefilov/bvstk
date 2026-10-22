// LAN8720A — Ethernet PHY. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="ethernet-rmii">
<title>LAN8720A — Ethernet PHY</title>
<rect x="5" y="17" width="23" height="30" rx="3"/><path d="M10 11v6m7-6v6m7-6v6M10 47v6m7-6v6m7-6v6M1 25h4m-4 14h4M28 27h10m-10 10h10"/>
<rect x="38" y="19" width="24" height="26" rx="2"/><path d="M43 25h14v11h-4v4h-6v-4h-4zM47 25v5m6-5v5M10 35l4-8 5 10 4-8"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
