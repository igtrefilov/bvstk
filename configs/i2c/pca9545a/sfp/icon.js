// SFP — оптический модуль. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="sfp">
<title>SFP — оптический модуль</title>
<path d="m9 24 13-15h33v29L42 53H9zM9 24h33l13-15M42 24v29"/>
<rect x="14" y="30" width="10" height="15" rx="1"/><rect x="28" y="30" width="9" height="15" rx="1"/>
<path d="M19 34v7m14-7v7M14 53v5h24v-5M26 14h20m-24 5h20M48 24v16"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
