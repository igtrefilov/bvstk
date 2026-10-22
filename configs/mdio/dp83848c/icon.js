// DP83848C — Ethernet PHY. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="ethernet-mii">
<title>DP83848C — Ethernet PHY</title>
<rect x="9" y="7" width="28" height="28" rx="3"/><path d="M16 2v5m7-5v5m7-5v5M2 14h7m-7 7h7m-7 7h7M16 35v6m7-6v6m7-6v6M37 14h6m-6 7h6"/>
<path d="M15 25h5v-8h6v8h5M37 29h15v8"/>
<rect x="27" y="38" width="34" height="22" rx="2"/><path d="M33 43h22v8h-5v4H38v-4h-5zM39 43v4m5-4v4m5-4v4"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
