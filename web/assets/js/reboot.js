import { apiReboot } from "./api.js";

function setStatus(btn, text) {
  const toolbar = btn.closest(".section-toolbar");
  const el = toolbar ? toolbar.querySelector(".reboot-status") : null;
  if (el) el.textContent = text;
}

export function initRebootButtons() {
  const buttons = document.querySelectorAll('[data-action="reboot"]');
  for (const btn of buttons) {
    btn.addEventListener("click", async () => {
      const ok = window.confirm("Перезагрузить устройство?\nСессия может оборваться.");
      if (!ok) {
        setStatus(btn, "Отменено");
        return;
      }

      try {
        setStatus(btn, "Перезагрузка...");
        await apiReboot({ confirm: true, delay_ms: 200 });
        setStatus(btn, "Перезагрузка...");
      } catch (e) {
        setStatus(btn, e?.message ? String(e.message) : String(e));
      }
    });
  }
}
