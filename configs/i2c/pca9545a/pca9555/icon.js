// PCA9555 — расширитель ввода и вывода. Device bundle icon API v1.
(() => {
  "use strict";
  let root;
  window.bvstkIcon = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" data-pictogram="gpio">
<title>PCA9555 — расширитель ввода и вывода</title>
<rect x="21" y="13" width="22" height="38" rx="3"/><path d="M27 6v7m10-7v7M27 51v7m10-7v7M4 22h17m-17 0 4-4m-4 4 4 4M4 32h17m-4-4 4 4-4 4M4 42h17m-17 0 4-4m-4 4 4 4M43 22h17m-4-4 4 4-4 4M43 32h17m-17 0 4-4m-4 4 4 4M43 42h17m-4-4 4 4-4 4M28 23h8m-8 9h8m-8 9h8"/>
</svg>`;
    },
    update(snapshot) {
      root.style.color = snapshot.appearance.color;
      root.dataset.presence = snapshot.device.presence;
      root.dataset.stale = String(snapshot.stale);
    }
  };
})();
