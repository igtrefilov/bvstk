// P24C02A — память EEPROM. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="memory">
<title>P24C02A — память EEPROM</title>
<rect x="14" y="8" width="36" height="48" rx="4"/><path d="M27 8a5 5 0 0 0 10 0M5 17h9M5 27h9M5 37h9M5 47h9m36-30h9m-9 10h9m-9 10h9m-9 10h9"/>
<rect x="22" y="20" width="20" height="24" rx="1"/><path d="M22 28h20m-20 8h20M32 20v24"/><circle cx="21" cy="49" r="1" fill="currentColor" stroke="none"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
