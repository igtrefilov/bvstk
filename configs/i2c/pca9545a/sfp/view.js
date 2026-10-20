// SFF-8472 Rev 12.5a, sections 4, 5, 8 and 9.
// Cache-only presentation: no bus operations, timers, network or external code.
(function () {
  "use strict";
  const MAX_PAIR_SKEW_MS = 1000; // Reject widely separated bytes; not an atomicity guarantee.
  const signed16 = n => n >= 0x8000 ? n - 0x10000 : n;
  const hex = n => "0x" + n.toString(16).toUpperCase().padStart(2, "0");
  const metrics = [
    {id:"temperature", label:"Температура модуля", reg:96, scale:1/256, unit:"°C", digits:2, slope:84, signed:true},
    {id:"voltage", label:"Напряжение питания", reg:98, scale:0.0001, unit:"В", digits:3, slope:88},
    {id:"bias", label:"Ток смещения TX", reg:100, scale:0.002, unit:"мА", digits:3, slope:76},
    {id:"tx_power", label:"Оптическая мощность TX", reg:102, scale:0.0001, unit:"мВт", digits:4, slope:80},
    {id:"rx_power", label:"Оптическая мощность RX", reg:104, scale:0.0001, unit:"мВт", digits:4}
  ];
  const flagNames = ["Температура выше", "Температура ниже", "Питание выше", "Питание ниже",
    "Ток TX выше", "Ток TX ниже", "Мощность TX выше", "Мощность TX ниже",
    "Мощность RX выше", "Мощность RX ниже", "Температура лазера или длина волны выше",
    "Температура лазера или длина волны ниже", "Ток TEC выше", "Ток TEC ниже"];

  function decode(snapshot) {
    const cache = new Map((snapshot.cache || []).map(e => [e.reg, e]));
    const spaces = new Map((snapshot.device?.address_spaces || []).map(s => [s.id, s]));
    const result = {rows:{}, count:0, message:"", stale:!!snapshot.stale};
    function entry(space, reg) {
      const s = spaces.get(space), e = s && reg < s.register_count ? cache.get(s.first + reg) : null;
      return e?.valid === true && ["I2C", "read"].includes(e.source) &&
        Number.isInteger(e.val) && e.val >= 0 && e.val <= 255 ? e : null;
    }
    function bytes(space, reg, count, dynamic = false) {
      const entries = Array.from({length:count}, (_, i) => entry(space, reg + i));
      if (entries.some(e => !e)) return null;
      const times = entries.map(e => e.updated_ms);
      if (dynamic && (times.some(t => !Number.isFinite(t) || t < 0) ||
          Math.max(...times) - Math.min(...times) > MAX_PAIR_SKEW_MS)) return null;
      return {values:entries.map(e => e.val),
        time:times.every(Number.isFinite) ? Math.min(...times) : null};
    }
    function byte(space, reg) { return entry(space, reg)?.val ?? null; }
    function word(reg, dynamic = false) {
      const b = bytes("diagnostics", reg, 2, dynamic);
      return b ? {...b, value:b.values[0] * 256 + b.values[1]} : null;
    }
    function set(id, label, text, tone = "offline", time = null, value = null) {
      result.rows[id] = {label, text:text ?? "Нет данных", tone:text == null ? "unknown" : tone, time, value};
    }
    function ascii(id, label, reg, count) {
      const b = bytes("identity", reg, count);
      const text = b && b.values.every(v => v >= 32 && v <= 126)
        ? String.fromCharCode(...b.values).trim() || "Не указано" : null;
      set(id, label, text, "offline", b?.time);
    }
    for (const s of spaces.values()) for (let r = 0; r < s.register_count; r++)
      if (entry(s.id, r)) result.count++;
    const identifier = byte("identity", 0);
    if (identifier !== 3) {
      result.message = identifier == null
        ? "Паспорт модуля ещё не прочитан. Показатели появятся по мере поступления данных в кэш."
        : "Идентификатор " + hex(identifier) + " не соответствует SFP. Расшифровка остановлена.";
      return result;
    }
    ascii("vendor", "Изготовитель", 20, 16);
    ascii("part", "Модель", 40, 16);
    ascii("revision", "Ревизия", 56, 4);
    ascii("serial", "Серийный номер", 68, 16);
    ascii("date", "Дата YYMMDD и партия", 84, 8);
    const oui = bytes("identity", 37, 3);
    set("oui", "OUI изготовителя", oui?.values.map(v => hex(v).slice(2)).join("-"), "offline", oui?.time);
    const nominal = byte("identity", 12), extendedRate = byte("identity", 66);
    const rate = nominal == null || nominal === 255 && extendedRate == null ? null
      : nominal === 255 ? extendedRate * 250 : nominal * 100;
    set("rate", "Номинальная символьная скорость", rate == null ? null : rate ? rate + " МБод" : "Не указана",
      "offline", entry("identity", nominal === 255 ? 66 : 12)?.updated_ms);
    const caps = byte("identity", 92), options = byte("identity", 93), status = byte("diagnostics", 110);
    const ddm = caps == null ? null : !!(caps & 0x40);
    const external = caps != null && !!(caps & 0x10), internal = caps != null && !!(caps & 0x20);
    const addressChange = caps != null && !!(caps & 4);
    const calibration = ddm === null ? null : !ddm ? "Не применяется"
      : internal === external ? "Не определена" : external ? "Внешняя" : "Внутренняя";
    set("ddm", "Цифровая диагностика DDM", ddm == null ? null : ddm ? "Поддерживается" : "Не поддерживается");
    set("calibration", "Калибровка", calibration, internal === external && ddm ? "warn" : "offline");
    set("rx_type", "Измерение мощности RX", ddm ? caps & 8 ? "Средняя мощность" : "OMA" : null);
    const reason = ddm === null ? "Нет данных о поддержке DDM" : !ddm ? "DDM не поддерживается"
      : addressChange ? "Требуется неподдерживаемое переключение адреса"
      : internal === external ? "Не определён способ калибровки"
      : status == null ? "Нет данных о готовности DDM" : status & 1 ? "Модуль готовит данные DDM" : null;
    set("ready", "Данные DDM", ddm && !addressChange && status != null ? status & 1 ? "Не готовы" : "Готовы" : null,
      status & 1 ? "warn" : "offline", entry("diagnostics", 110)?.updated_ms);
    const state = (id, label, option, mask, zero, one, tone) => {
      const supported = options == null ? null : !!(options & option);
      set(id, label, !ddm || addressChange || supported == null ? null : !supported ? "Не поддерживается"
        : status == null ? null : status & mask ? one : zero,
      supported && status != null && status & mask ? tone : "offline", entry("diagnostics", 110)?.updated_ms);
    };
    state("los", "Приём сигнала RX_LOS", 0x10, 2, "Сигнал обнаружен", "Потеря сигнала", "warn");
    state("fault", "Ошибка передатчика TX_FAULT", 0x20, 4, "Нет", "Есть", "bad");
    state("disable", "Программный TX_DISABLE", 0x40, 0x40, "Снят", "Установлен", "warn");

    // External RX polynomial produces the same 0.1 µW units as internal calibration
    // (SFF-8472 12.5a, 9.3). All five IEEE-754 big-endian coefficients are required.
    function calibrated(metric, raw) {
      let value = metric.signed ? signed16(raw) : raw;
      if (external) {
        if (metric.id === "rx_power") {
          let poly = 0;
          for (let reg = 56; reg <= 72; reg += 4) {
            const b = bytes("diagnostics", reg, 4);
            if (!b) return null;
            const coefficient = new DataView(Uint8Array.from(b.values).buffer).getFloat32(0, false);
            if (!Number.isFinite(coefficient)) return null;
            poly = poly * value + coefficient;
          }
          value = poly;
        } else {
          const slope = word(metric.slope), offset = word(metric.slope + 2);
          if (!slope || !offset) return null;
          value = value * (slope.value / 256) + signed16(offset.value);
        }
      }
      value *= metric.scale;
      return Number.isFinite(value) && (metric.signed || value >= 0) ? value : null;
    }
    function formatted(metric, value) {
      return value.toFixed(metric.digits) + " " + metric.unit +
        (metric.unit === "мВт" && value > 0 ? " (" + (10 * Math.log10(value)).toFixed(2) + " dBm)" : "");
    }
    for (const [i, metric] of metrics.entries()) {
      const w = word(metric.reg, true);
      const txDisabled = metric.id === "tx_power" && status != null &&
        (status & 0x80 || options != null && options & 0x40 && status & 0x40);
      const value = !reason && !txDisabled && w ? calibrated(metric, w.value) : null;
      set(metric.id, metric.label, reason || (txDisabled ? "Передатчик отключён" : value == null
        ? null : formatted(metric, value)), value == null ? "unknown" : "offline", w?.time, value);
      for (const [j, level] of ["Авария выше", "Авария ниже", "Предупреждение выше", "Предупреждение ниже"].entries()) {
        const threshold = word(i * 8 + j * 2);
        const t = ddm && !addressChange && internal !== external && threshold
          ? calibrated(metric, threshold.value) : null;
        set(metric.id + "_limit" + j, level, t == null ? null : formatted(metric, t), "offline", threshold?.time, t);
      }
    }
    for (const [id, label, reg] of [["alarms", "Флаги аварий", 112], ["warnings", "Флаги предупреждений", 116]]) {
      const b = bytes("diagnostics", reg, 2, true);
      const supported = options != null && !!(options & 0x80);
      const flags = b ? (b.values[0] * 256 + b.values[1]) & 0xFFFC : null;
      const labels = flags == null ? [] : flagNames.filter((_, i) => flags & (1 << (15 - i)));
      set(id, label, reason || (options == null ? null : !supported ? "Не поддерживаются"
        : flags == null ? null : labels.join("; ") || "Нет установленных"),
      flags && supported && !reason ? id === "alarms" ? "bad" : "warn" : "offline", b?.time);
    }
    for (const [id, label, first, end] of [["base_crc", "Сумма паспорта CC_BASE", 0, 63],
      ["ext_crc", "Сумма опций CC_EXT", 64, 95]]) {
      const b = bytes("identity", first, end - first + 1);
      const matches = b && (b.values.slice(0, -1).reduce((n, v) => n + v, 0) & 255) === b.values.at(-1);
      set(id, label, b ? matches ? "Совпадает" : "Не совпадает" : null, matches ? "ok" : "warn", b?.time);
    }
    result.message = reason || "Расшифровка последних подтверждённых значений кэша.";
    if (snapshot.device?.presence === "unavailable") result.message = "Устройство не готово. Показаны ранее прочитанные данные.";
    if (result.stale) result.message = "Связь с bvstk потеряна. Показан последний полученный снимок.";
    return result;
  }

  window.bvstkView = {
    apiVersion:1,
    decode,
    mount(root) {
      const style = document.createElement("style");
      style.textContent = `body{margin:0;color:#242c36;font:14px system-ui,sans-serif}
        h2{font-size:16px;margin:22px 0 12px}h2:first-of-type{margin-top:14px}
        .notice,.foot{color:#66717f;font-size:12px;line-height:1.55;margin:8px 0}
        .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:10px}
        .cell{border:1px solid #dce2e8;border-radius:5px;padding:12px;min-width:0}
        .label{font-size:12px;color:#66717f}.value{font-size:15px;font-weight:600;margin-top:6px;overflow-wrap:anywhere}
        .unknown{color:#798592}.ok{color:#237247}.bad{color:#b33137}.warn{color:#946308}
        .time{font-size:11px;color:#798592;margin-top:5px}
        .limits{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:12px}
        th,td{text-align:left;border-bottom:1px solid #e1e6eb;padding:10px;min-width:115px}th{font-weight:500;color:#66717f}`;
      root.append(style);
      this.notice = document.createElement("p"); this.notice.className = "notice"; root.append(this.notice);
      this.cells = new Map();
      const section = (title, ids) => {
        const h = document.createElement("h2"); h.textContent = title; root.append(h);
        const grid = document.createElement("div"); grid.className = "grid"; root.append(grid);
        for (const [id, label] of ids) {
          const cell = document.createElement("div"); cell.className = "cell"; cell.dataset.metric = id;
          const name = document.createElement("div"); name.className = "label"; name.textContent = label;
          const value = document.createElement("div"); value.className = "value"; value.textContent = "Нет данных";
          const time = document.createElement("div"); time.className = "time";
          cell.append(name, value, time); grid.append(cell); this.cells.set(id, {value, time});
        }
      };
      section("Паспорт модуля", [["vendor","Изготовитель"],["part","Модель"],["revision","Ревизия"],
        ["serial","Серийный номер"],["date","Дата YYMMDD и партия"],["oui","OUI изготовителя"],["rate","Номинальная символьная скорость"]]);
      section("Цифровая диагностика", [["ddm","Поддержка DDM"],["calibration","Калибровка"],
        ["rx_type","Измерение мощности RX"],["ready","Данные DDM"]]);
      section("Измерения по кэшу", metrics.map(m => [m.id, m.label]));
      section("Состояние", [["los","Приём сигнала RX_LOS"],["fault","Ошибка передатчика TX_FAULT"],
        ["disable","Программный TX_DISABLE"],["alarms","Флаги аварий"],["warnings","Флаги предупреждений"]]);
      const h = document.createElement("h2"); h.textContent = "Пороги изготовителя"; root.append(h);
      const wrap = document.createElement("div"); wrap.className = "limits";
      const table = document.createElement("table"), head = document.createElement("tr");
      for (const label of ["Параметр","Авария выше","Авария ниже","Предупреждение выше","Предупреждение ниже"]) {
        const th = document.createElement("th"); th.textContent = label; head.append(th);
      }
      table.append(head);
      for (const m of metrics) {
        const tr = document.createElement("tr"), name = document.createElement("th");
        name.textContent = m.label; tr.append(name);
        for (let i = 0; i < 4; i++) {
          const value = document.createElement("td"); value.textContent = "Нет данных"; tr.append(value);
          this.cells.set(m.id + "_limit" + i, {value});
        }
        table.append(tr);
      }
      wrap.append(table); root.append(wrap);
      section("Контроль паспорта", [["base_crc","Сумма паспорта CC_BASE"],["ext_crc","Сумма опций CC_EXT"]]);
      const foot = document.createElement("p"); foot.className = "foot";
      foot.textContent = "Форма не опрашивает шину. Метки времени отсчитываются от старта bvstk. " +
        "Байты измерений читаются отдельно: расчёт приблизительный, целостность снимка не гарантируется; " +
        "при разнице меток двух байтов более 1 с число скрывается. " +
        "RX_LOS не определяет Ethernet link, номинальная скорость относится к паспорту модуля. " +
        "Флаги могут быть защёлкнуты; активный флаг следует подтвердить чтением через 100 мс. " +
        "Дополнительные страницы, адресное переключение, расширенные опции и параметры лазера зависят от исполнения модуля.";
      root.append(foot);
    },
    update(snapshot) {
      const data = decode(snapshot);
      this.notice.textContent = data.message + " В кэше " + data.count + " из 256 байт.";
      for (const [id, cell] of this.cells) {
        const row = data.rows[id];
        cell.value.textContent = row?.text || "Нет данных";
        cell.value.className = "value " + (data.stale ? "unknown" : row?.tone || "unknown");
        cell.value.title = Number.isFinite(row?.time) ? "Метка времени: " + row.time + " мс" : "";
        if (cell.time) cell.time.textContent = Number.isFinite(row?.time) ? row.time + " мс" : "";
      }
    }
  };
}());
