// PCA9545A — четырёхканальный I²C-свитч. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="switch">
<title>PCA9545A — четырёхканальный I²C-свитч</title>
<rect x="6" y="23" width="13" height="18" rx="2"/><path d="M2 28h4m-4 8h4M19 32h8V9h15M27 24h15M27 40h15M27 32v23h15"/>
<rect x="42" y="4" width="16" height="10" rx="2"/><rect x="42" y="19" width="16" height="10" rx="2"/>
<rect x="42" y="35" width="16" height="10" rx="2"/><rect x="42" y="50" width="16" height="10" rx="2"/>
<path d="M58 9h4m-4 15h4m-4 16h4m-4 15h4"/><circle cx="27" cy="32" r="2" fill="currentColor" stroke="none"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
