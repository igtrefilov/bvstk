import { apiReboot } from "./api.js";

function setStatus(btn, text) {
  const toolbar = btn.closest(".section-toolbar");
  const el = toolbar ? toolbar.querySelector(".reboot-status") : null;
  if (el) el.textContent = text;
}

function isExpectedFetchDrop(e) {
  const msg = e?.message ? String(e.message) : String(e);
  return msg.includes("Failed to fetch") || msg.includes("NetworkError") || msg.includes("Load failed");
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
        btn.disabled = true;
        await apiReboot({ confirm: true, delay_ms: 1500 });
        setStatus(btn, "Команда отправлена");
      } catch (e) {
        if (isExpectedFetchDrop(e)) {
          setStatus(btn, "Перезагрузка...");
          return;
        }
        setStatus(btn, e?.message ? String(e.message) : String(e));
      } finally {
        btn.disabled = false;
      }
    });
  }
}
