// AXP15060 — управление питанием. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="power">
<title>AXP15060 — управление питанием</title>
<rect x="14" y="14" width="36" height="36" rx="4"/>
<path d="M22 6v8m10-8v8m10-8v8M22 50v8m10-8v8m10-8v8M6 22h8M6 32h8M6 42h8m36-20h8m-8 10h8m-8 10h8"/>
<path d="m34 21-11 14h9l-2 9 11-15h-9z"/><circle cx="20" cy="20" r="1" fill="currentColor" stroke="none"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
