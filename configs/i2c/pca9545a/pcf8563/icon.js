// PCF8563 — часы реального времени. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="rtc">
<title>PCF8563 — часы реального времени</title>
<rect x="12" y="12" width="40" height="40" rx="4"/><path d="M21 5v7m11-7v7m11-7v7M21 52v7m11-7v7m11-7v7M5 21h7M5 32h7M5 43h7m40-22h7m-7 11h7m-7 11h7"/>
<circle cx="32" cy="32" r="13"/><path d="M32 23v10l7 4M32 19v2m13 11h-2M32 45v-2M19 32h2"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
