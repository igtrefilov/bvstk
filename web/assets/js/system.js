import * as api from "./api.js";
import {esc, head, kv, bytes, uptime, ask, notice, footer, action} from "./ui.js";
export async function systemPage(root, params, ctx) {
  let dirty = false;
  let tab = params.get("tab") || "network";
  if (!["network","runtime","maintenance"].includes(tab)) tab = "network";
  async function refresh() {
    if (dirty && !await ask({title:"Отбросить черновик?",body:"<p>Несохранённые поля сети будут загружены заново.</p>"})) return;
    const net = await api.get("/api/net", ctx.signal), rtos = await api.get("/api/rtos", ctx.signal), ver = await api.get("/api/version", ctx.signal);
    if (ctx.signal.aborted) return; dirty = false;
    root.innerHTML = `${head("Система", "Сеть, состояние FreeRTOS и обслуживание")}<div class="tabs" aria-label="Разделы системы"><button data-tab="network" class="active">Сеть</button><button data-tab="runtime">FreeRTOS</button><button data-tab="maintenance">Обслуживание</button></div><div class="stack">
    <article class="card" data-section="network"><h2>Сеть</h2><p class="muted">Настройки сохраняются во Flash. Применение нового IP может прервать соединение.</p><form id="network-form" class="form-grid">${[["ip","IP-адрес",net.ip],["netmask","Маска сети",net.netmask],["gateway","Шлюз",net.gateway],["mac","MAC-адрес",net.mac]].map(([key,label,val]) => `<label>${label}<input name="${key}" value="${esc(val)}" required autocomplete="off"></label>`).join("")}<label class="check wide"><input name="apply" type="checkbox" checked>Применить сразу</label><div class="actions wide"><button class="primary" type="submit">Сохранить настройки сети</button><span id="network-state" class="muted"></span></div></form><div id="network-reconnect"></div></article>
    <article class="card hidden" data-section="runtime"><h2>FreeRTOS</h2>${kv([["Время работы",uptime(rtos.uptime_ms)],["Свободная память",bytes(rtos.heap_free)],["Минимум свободной памяти",bytes(rtos.heap_min_ever)],["Частота тиков",rtos.tick_rate_hz + " Гц"],["Сборка",ver.build_date + " " + ver.build_time],["Порт HTTP",ver.http_port]])}</article>
    <article class="card hidden" data-section="maintenance"><h2>Обслуживание</h2><p>Перезагрузка остановит активные операции. Изменения подсистем, применённые только в RAM, будут потеряны.</p><button id="reboot" class="danger">Перезагрузить систему</button><p class="muted">HTTP-доступ предназначен для доверенной сети. Шифрование и аутентификация этим сервером не предоставляются.</p></article></div>${footer()}`;
    const selectTab = value => {
      tab = value; ctx.setContext({tab});
      root.querySelectorAll("[data-tab]").forEach(t => t.classList.toggle("active", t.dataset.tab === tab));
      root.querySelectorAll("[data-section]").forEach(t => t.classList.toggle("hidden", t.dataset.section !== tab));
    };
    root.querySelectorAll("[data-tab]").forEach(b => b.onclick = () => selectTab(b.dataset.tab));
    selectTab(tab); root.classList.remove("is-stale");
    const form = root.querySelector("#network-form");
    form.oninput = () => { dirty = true; root.querySelector("#network-state").textContent = "Есть неприменённые изменения"; };
    form.onsubmit = event => { event.preventDefault(); action(form.querySelector("button"), async () => {
      const data = Object.fromEntries(new FormData(form)); data.apply = form.elements.apply.checked;
      for (const key of ["ip","netmask","gateway"]) {
        if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(data[key]) || data[key].split(".").some(n => Number(n) > 255)) throw new Error("Неверный IPv4: " + key);
      }
      if (!/^[\da-f]{2}(:[\da-f]{2}){5}$/i.test(data.mac)) throw new Error("Неверный MAC-адрес");
      await ask({title:"Сохранить сеть?",body:`<p>Адрес устройства: <strong>${esc(data.ip)}</strong></p><p>${data.apply ? "Новые параметры вступят в силу сразу. Ответ может не успеть дойти." : "Новые параметры вступят в силу после перезагрузки."}</p>`,submit:"Сохранить",run:async () => {
        if (data.apply && data.ip !== net.ip) {
          const target = new URL(location.href); target.hostname = data.ip;
          root.querySelector("#network-reconnect").innerHTML = `<p class="info-strip">После применения откройте <a href="${esc(target.href)}">${esc(data.ip)}</a>. Не повторяйте запрос без проверки нового адреса.</p>`;
        }
        const result = await api.request("/api/net", {method:"PUT",body:data});
        dirty = !result.saved;
        const message = `Сохранение: ${result.saved ? "успешно" : "ошибка"}. Применение: ${result.applied ? "выполнено" : "не выполнено"}.`;
        root.querySelector("#network-state").textContent = message; notice(message, !result.saved);
      }});
    }); };
    root.querySelector("#reboot").onclick = () => ask({title:"Перезагрузить bvstk?",body:"<p>Сначала завершите передачу файлов и сохраните конфигурации I²C и MDIO. Несохранённые изменения в RAM будут потеряны.</p>",submit:"Перезагрузить",danger:true,run:async () => {
      await api.request("/api/reboot",{method:"PUT",body:{confirm:true,delay_ms:1000}}); notice("Перезапуск запрошен. Дождитесь восстановления связи.");
    }});
  }
  await refresh(); return {refresh,poll:true,dirty:() => dirty};
}
