import * as api from "./api.js";
import {esc, hex, bytes, kv, head, badge, footer, stale, wireRows, deviceState} from "./ui.js";

export const volumePurpose = name => ({flash:"Конфигурация и веб-ресурсы",sd:"SD в PS","sd-pl":"SD в PL"}[name] || name);
export async function overview(root, params, ctx) {
  let net, rtos, version, volumes = [], objects = [], selected, detail, detailError = "", detailAt = "", fresh = "", ticket = 0;
  function inspector() {
    const item = objects.find(o => o.key === selected);
    if (!item) return '<h2>Выбранная подсистема</h2><div class="empty">Выберите строку таблицы</div>';
    const link = item.bus === "sd-pl" ? "#sd-pl" : `#${item.bus}?name=${encodeURIComponent(item.name)}`;
    if (item.bus === "sd-pl") {
      const v = volumes.find(v => v.name === "sd-pl");
      return `<h2>Выбранная подсистема</h2><div class="object-title">SD-PL<small>Внешняя SD-карта</small></div>${kv([["Том","sd-pl:/"],["Состояние",v?.ready ? "Смонтирован" : "Недоступен"],["Файловая система",v?.fat_bits ? "FAT"+v.fat_bits : "Не определена"],["Размер тома",bytes(v?.total_bytes)]])}<div class="actions"><a href="${link}">Состояние карты</a><a href="#files?volume=sd-pl">Открыть файлы</a></div>`;
    }
    if (item.empty) return `<h2>Выбранная подсистема</h2><div class="object-title">${item.label}</div>${kv([["Сервис",item.ready ? "Готово" : "Не готово"],["Устройства",item.ready ? "Нет настроенных устройств" : "Список недоступен"]])}<p><a href="#${item.bus}">Открыть подсистему</a></p>`;
    const config = detail?.config;
    return `<h2>Выбранная подсистема</h2><div class="object-title">${item.label}<small>${esc(item.name)}</small></div>${kv([["Адрес",hex(item.address)],["Политика",config?.policy || item.policy],...(item.bus === "mdio" ? [["Автоопрос",config ? config.autopoll_enabled ? "Включён" : "Выключен" : "Загрузка…"]] : []),["Конфигурация",detail ? detail.saved === true ? "Сохранена" : detail.saved === false ? "Не сохранена во Flash" : "Сохранение не проверено" : "Загрузка…"]])}${detailAt ? `<p><small>Снимок конфигурации ${esc(detailAt)}</small></p>` : ""}${detailError ? `<p class="error-text">${esc(detailError)}</p>` : ""}<div class="actions"><a class="button-link primary" href="${link}&tab=registers">Открыть регистры</a><a class="button-link" href="${link}&tab=policy">Политика</a></div>`;
  }
  async function inspect(key) {
    selected = key; detail = null; detailError = ""; detailAt = "";
    const currentTicket = ++ticket, item = objects.find(o => o.key === selected);
    render();
    if (!item || item.bus === "sd-pl" || item.empty) return;
    try {
      const result = await api.busGet(item.bus,item.name,ctx.signal);
      if (ctx.signal.aborted || currentTicket !== ticket) return;
      detail = result; detailAt = new Date().toLocaleTimeString("ru-RU");
    } catch(e) { if (ctx.signal.aborted || currentTicket !== ticket) return; detailError = e.message; }
    root.querySelector("#overview-inspector").innerHTML = inspector();
  }
  function render() {
    root.innerHTML = `<div class="workbench overview-workbench"><div class="stack"><article class="card"><div class="card-head"><h1>Состояние подсистем</h1></div><div class="table-scroll"><table aria-label="Состояние подсистем"><thead><tr><th>Подсистема</th><th>Устройство</th><th>Адрес</th><th>Состояние</th><th>Политика</th></tr></thead><tbody>${objects.map(item => `<tr class="selectable ${item.key === selected ? "selected" : ""}" tabindex="0" data-subsystem="${esc(item.key)}"><td><button class="text-button" data-inspect="${esc(item.key)}">${item.label}</button></td><td>${esc(item.name)}</td><td class="mono">${esc(item.bus === "sd-pl" ? "sd-pl:/" : hex(item.address))}</td><td>${badge(item.available ? "Готово" : "Не готово",item.available ? "ok" : "offline")}</td><td>${esc(item.policy || "—")}</td></tr>`).join("")}</tbody></table></div></article>
      <article class="card"><h2>Хранилища</h2><div class="table-scroll"><table><thead><tr><th>Том</th><th>Назначение</th><th>Состояние</th><th>Размер</th><th>Свободно</th></tr></thead><tbody>${volumes.map(v => `<tr><td><a class="mono" href="#files?volume=${encodeURIComponent(v.name)}">${esc(v.name)}:/</a></td><td class="wrap-text">${esc(volumePurpose(v.name))}</td><td>${badge(v.ready ? "Смонтирован" : "Недоступен",v.ready ? "ok" : "bad")}</td><td>${bytes(v.total_bytes)}</td><td>${v.free_bytes == null ? "Не определено" : bytes(v.free_bytes)}</td></tr>`).join("")}</tbody></table></div></article></div>
      <aside class="card inspector" id="overview-inspector">${inspector()}</aside></div>
      <div class="grid two" style="margin-top:12px"><article class="card"><h2>Сеть</h2>${kv([[net.mode === "dhcp" ? "DHCP" : "Статический IP",net.ip || "Нет адреса"],["Шлюз",net.gateway || "—"],["Линия",net.up && net.link_up ? "Поднята" : "Нет связи"]])}</article><article class="card"><h2>Среда исполнения</h2>${kv([["Система","FreeRTOS"],["Сборка",version.build_date+" "+version.build_time],["Частота тиков",rtos.tick_rate_hz+" Гц"]])}</article></div>${fresh}`;
    root.querySelectorAll("[data-inspect]").forEach(b => b.onclick = () => inspect(b.dataset.inspect));
    wireRows(root,"[data-subsystem]",row => inspect(row.dataset.subsystem));
  }
  async function refresh(automatic = false) {
    try {
      const data = await Promise.all([api.get("/api/net",ctx.signal),api.get("/api/rtos",ctx.signal),api.volumes(ctx.signal),api.busList("i2c",ctx.signal),api.busList("mdio",ctx.signal),version ? Promise.resolve(version) : api.get("/api/version",ctx.signal)]);
      if (ctx.signal.aborted) return;
      [net,rtos] = data; volumes = data[2].volumes; version = data[5];
      objects = data.slice(3,5).flatMap((list,i) => {
        const bus = i ? "mdio" : "i2c", label = i ? "MDIO" : "I²C";
        if (!list.devices.length) return [{bus,label,key:bus+":",name:"—",empty:true,ready:list.ready,available:false,state:list.ready ? "Устройства не настроены" : "Сервис не готов"}];
        return list.devices.map(d => {
          const state = deviceState(bus,d);
          return {...d,bus,label,key:bus+":"+d.name,available:state.ready,state:state.label};
        });
      });
      const sd = volumes.find(v => v.name === "sd-pl");
      objects.push({bus:"sd-pl",key:"sd-pl",label:"SD-PL",name:"Внешняя SD-карта",available:sd?.ready,state:sd?.ready ? "Том смонтирован" : "Недоступен",policy:sd?.fat_bits ? "FAT"+sd.fat_bits : "—"});
      if (!objects.some(o => o.key === selected)) { ticket++; selected = objects.find(o => o.bus === "mdio")?.key || objects[0]?.key; detail = null; detailAt = ""; detailError = ""; }
      root.classList.remove("is-stale"); fresh = footer(); render();
      // Configuration snapshots read Flash; do not repeat these on every poll.
      if (!automatic && selected) await inspect(selected);
    } catch(e) { if (root.querySelector("[data-fresh]")) stale(root,e); else throw e; }
  }
  await refresh(); return {refresh,poll:true};
}
export async function sdPage(root, params, ctx) {
  async function refresh() {
    const data = await api.get("/api/ui/sd-pl", ctx.signal);
    if (ctx.signal.aborted) return;
    root.classList.remove("is-stale");
    root.innerHTML = `${head("SD-PL", "Внешняя SD-карта через PL")}<div class="stack"><article class="card"><div class="card-head"><h2>Состояние карты</h2>${badge(data.mounted ? "Смонтирован" : "Недоступен", data.mounted ? "ok" : "bad")}</div>${kv([["Карта инициализирована",data.ready ? "Да" : "Нет"],["Том","sd-pl:/"],["Файловая система", data.fat_bits ? "FAT" + data.fat_bits : "Не определена"],["Адресация", data.block_addressed ? "LBA" : "Не определена"],["Размер тома", bytes(data.volume_sectors == null ? null : data.volume_sectors * 512)],["Последняя операция",data.last_error],["Последний LBA",data.last_lba]])}<p><a href="#files?volume=sd-pl">Открыть файлы →</a></p></article><div class="info-strip">Работа с существующей файловой системой. Форматирование отключено. Размер тома не равен физической ёмкости карты. После ошибки носителя может потребоваться перезапуск системы.</div></div>${footer()}`;
  }
  await refresh(); return {refresh, poll: true};
}
