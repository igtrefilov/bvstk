// BVSTK device-view API 1. Layout only; register meaning lives in the JSON map.
// No register masks, voltage formulas, network requests or physical commands.
(() => {
  let root;
  const time = ms => {
    if (!Number.isFinite(ms)) return "—";
    const s = Math.floor(ms / 1000), days = Math.floor(s / 86400);
    return (days ? days + " д. " : "") + [Math.floor(s / 3600) % 24, Math.floor(s / 60) % 60, s % 60]
      .map(n => String(n).padStart(2, "0")).join(":") + "." + String(ms % 1000).padStart(3, "0");
  };
  const value = (kind, key) => `<span data-${kind}="${key}" class="datum"><strong data-value class="unknown">Нет данных</strong><small data-time>Метка времени —</small></span>`;
  const tile = (title, kind, key) => `<section><span class="label">${title}</span>${value(kind,key)}</section>`;
  const linear = [...Array.from({length:5},(_,i)=>"aldo"+(i+1)), ...Array.from({length:5},(_,i)=>"bldo"+(i+1)),
    ...Array.from({length:4},(_,i)=>"cldo"+(i+1)), "cpusldo"];
  const interrupts = [
    ...Array.from({length:6},(_,i)=>["1","dcdc"+(i+1),"DCDC"+(i+1)+" ниже 85% задания"]),
    ["1","temperature1","Температурный уровень 1"], ["1","temperature2","Температурный уровень 2"],
    ["2","gpio1","GPIO1"], ["2","gpio2","GPIO2"], ["2","pok_fall","Спадающий фронт POK"],
    ["2","pok_rise","Нарастающий фронт POK"], ["2","pok_short","Короткое нажатие POK"], ["2","pok_long","Длительное нажатие POK"]
  ];
  window.bvstkView = {
    apiVersion: 1,
    mount(target) {
      root = target;
      const style = document.createElement("style");
      style.textContent = `
        h3{font-size:15px;margin:24px 0 12px}p{color:#697985;font-size:12px;line-height:1.5;margin:8px 0 14px}
        .tiles{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}.tiles section{border:1px solid #d7dfe4;background:#f6f8f9;padding:14px}
        .label{display:block;color:#586874;font-size:12px;margin-bottom:8px}.datum{display:block;min-width:0}.datum strong{font-size:13px;font-weight:600;overflow-wrap:anywhere}
        small{display:block;color:#73818c;font-size:10px;margin-top:5px;font-variant-numeric:tabular-nums}.table-wrap{overflow-x:auto;border:1px solid #d7dfe4}
        table{width:100%;border-collapse:collapse;text-align:left;font-size:12px}th,td{padding:11px 12px;border-bottom:1px solid #e0e6ea;vertical-align:top}thead{background:#edf1f4}th{font-weight:600;color:#455662}
        tbody th{white-space:nowrap;background:#f8fafb}tbody tr:last-child>*{border-bottom:0}.rails{min-width:600px}.linear{min-width:480px}.irq{min-width:470px}
        .modes{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px;margin-top:14px}.modes section{padding:10px;border-left:2px solid #cbd5dc}
        .sources{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:14px}.sources section{border:1px solid #d7dfe4;padding:12px}
        details{margin-top:20px;border:1px solid #d7dfe4;padding:14px}summary{cursor:pointer;font-weight:600;font-size:14px}details .tiles{margin-top:16px}
        .ok{color:#247840}.bad{color:#ac342b}.warn{color:#886213}.unknown{color:#68737c}.offline{color:#24313a}.muted{color:#73818c}
        @media(max-width:750px){.tiles,.sources{grid-template-columns:repeat(2,minmax(0,1fr))}.modes{grid-template-columns:1fr}.rails{min-width:580px}th,td{padding:10px 8px}}
      `;
      document.head.append(style);
      root.innerHTML = `
        <div class="tiles">
          ${tile("Порог отключения VOFF","summary","voff")}
          ${tile("Защита от перенапряжения","summary","ovp")}
          ${tile("Температурный IRQ уровня 1","field","irq_status1.temperature1")}
          ${tile("Температурный IRQ уровня 2","field","irq_status1.temperature2")}
        </div>
        <p>Напряжения — задания, не измерения. Разрешение канала — настройка управления, не подтверждение питания на выходе. IRQ показывает кэш флагов, а не измеренную температуру.</p>
        <h3>Преобразователи DC-DC</h3>
        <div class="table-wrap"><table class="rails"><thead><tr><th>Канал</th><th>Разрешение</th><th>Заданное напряжение</th><th>Режим</th><th>Флаг пониженного напряжения</th></tr></thead><tbody>
        ${Array.from({length:6},(_,i)=>{const id="dcdc"+(i+1);return `<tr data-rail="${id}"><th>${id.toUpperCase()}</th><td>${value("summary",id+"_enable")}</td><td>${value("summary",id+"_voltage")}</td><td>${value("summary",id+"_mode")}</td><td>${value("field","irq_status1."+id)}</td></tr>`;}).join("")}
        </tbody></table></div>
        <div class="modes">${tile("DCDC2 и DCDC3","summary","poly23")}${tile("DCDC4 и DCDC6","summary","poly46")}${tile("Время одного шага DVM","summary","dvm_ramp")}</div>
        <p>В двухфазной паре показаны настройки ведущего канала. Если режим пары не попал в кэш, зависимые значения неизвестны. Отдельные коды каждого канала доступны ниже в расшифровке регистров.</p>
        <h3>Линейные выходы и ключ</h3>
        <div class="table-wrap"><table class="linear"><thead><tr><th>Канал</th><th>Разрешение</th><th>Задание по регистру</th><th>Назначение</th></tr></thead><tbody>
        ${linear.map(id=>`<tr data-rail="${id}"><th>${id.toUpperCase()}</th><td>${value("summary",id+"_enable")}</td><td>${value("summary",id+"_voltage")}</td><td>${id==="cldo3"?value("summary","gpio1"):id==="cldo4"?value("summary","gpio2"):id==="cpusldo"?"CPUSLDO":"LDO"}</td></tr>`).join("")}
        <tr data-rail="switch"><th>DC1SW</th><td>${value("summary","switch_enable")}</td><td class="muted">Нет отдельного задания</td><td>Ключ от DCDC1</td></tr>
        </tbody></table></div>
        <p>Для высокого уровня GPIO1 и GPIO2 документация задаёт диапазон 1,25–3,3 В. RTCLDO не имеет описанного регистра управления: его фактическое состояние и напряжение по этой карте не определяются. VDDR/2 — режим слежения CPUSLDO, без подстановки выдуманного числового напряжения.</p>
        <h3>Признаки включения</h3>
        <div class="sources">
          ${tile("ALDOIN","field","power_on.aldoin")}${tile("PWRON","field","power_on.pok")}${tile("IRQ","field","power_on.irq")}
          ${tile("EN при ALDOINGOOD","field","power_on.en")}${tile("ALDOINGOOD при EN","field","power_on.aldoin_good_en")}
        </div>
        <details data-section="interrupts"><summary>Прерывания и защитные события</summary>
          <p>Флаги очищаются записью единицы. Для REG 0x48 они также очищаются при восстановлении контролируемого параметра. Нулевой флаг не доказывает отсутствие прошлой аварии. Кэш команды очистки не подтверждает событие.</p>
          <div class="table-wrap"><table class="irq"><thead><tr><th>Причина</th><th>Разрешение IRQ</th><th>Флаг в кэше</th></tr></thead><tbody>
            ${interrupts.map(([bank,id,title])=>`<tr><th>${title}</th><td>${value("field","irq_enable"+bank+"."+id)}</td><td>${value("field","irq_status"+bank+"."+id)}</td></tr>`).join("")}
          </tbody></table></div>
        </details>
        <details data-section="controls"><summary>Параметры защит и управления</summary><div class="tiles">
          ${tile("Контроль температуры","summary","temperature")}${tile("Отключение при уровне 2","summary","overtemperature")}
          ${tile("Разряд выходов при выключении","summary","discharge")}${tile("Порядок выключения","summary","off_sequence")}
          ${Array.from({length:6},(_,i)=>tile("Отключение при UVP DCDC"+(i+1),"field","protection.uvp"+(i+1))).join("")}
          ${tile("Пробуждение","summary","wakeup")}${tile("Пробуждение по IRQ","summary","irq_wakeup")}
          ${tile("Напряжения после пробуждения","field","wakeup.voltage")}${tile("Задержка PWROK","field","power_good.pwrok_delay")}
          ${tile("Удержание POK для включения","field","pok.on_time")}${tile("Удержание POK для выключения","field","pok.off_time")}
          ${tile("Удержание POK для IRQ","field","pok.irq_time")}${tile("Повторное включение после POK","field","pok.restart")}
          ${[2,3,4,5].map(n=>tile("Флаг DVM DCDC"+n,"field","dcdc_mode1.dvm"+n)).join("")}
        </div><p>Флаги DVM перечислены как отдельные настройки регистров. В двухфазном режиме параметры задаёт ведущий DC-DC. Эти данные не доказывают, что PMIC сейчас находится в режиме сна, перехода или выключения.</p></details>
      `;
    },
    update(snapshot) {
      const summary = new Map(snapshot.summary.map(item => [item.id,item]));
      for (const node of root.querySelectorAll("[data-summary], [data-field]")) {
        const item = node.dataset.summary ? summary.get(node.dataset.summary) : snapshot.fields[node.dataset.field];
        const text = node.querySelector("[data-value]");
        text.textContent = item?.label || "Нет данных";
        text.className = item?.tone || "unknown";
        node.querySelector("[data-time]").textContent = "Метка времени " + time(item?.updated_ms);
      }
    }
  };
})();
