// KSZ8081RNA — Ethernet PHY. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="ethernet-transceiver">
<title>KSZ8081RNA — Ethernet PHY</title>
<rect x="15" y="16" width="34" height="32" rx="4"/><path d="M22 8v8m10-8v8m10-8v8M22 48v8m10-8v8m10-8v8M6 24h9M6 40h9m34-16h9m-9 16h9"/>
<path d="M21 27h22m-5-5 5 5-5 5M43 38H21m5-5-5 5 5 5"/>
<circle cx="6" cy="24" r="2"/><circle cx="6" cy="40" r="2"/><circle cx="58" cy="24" r="2"/><circle cx="58" cy="40" r="2"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
