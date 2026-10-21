// SFF-8472 Rev 12.5a, sections 4, 5, 8 and 9.
// Cache-only presentation: no bus operations, timers, network or external code.
(function () {
  "use strict";
  const LIVE_MAX_AGE_MS = 5000;
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
    const result = {rows:{}, count:0, message:"", stale:!!snapshot.stale,
      now:snapshot.uptime_ms, presence:snapshot.device?.presence || "unknown"};
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
          Math.max(...times) !== Math.min(...times))) return null;
      return {values:entries.map(e => e.val),
        time:times.every(Number.isFinite) ? Math.min(...times) : null};
    }
    function byte(space, reg) { return entry(space, reg)?.val ?? null; }
    function word(reg, dynamic = false) {
      const b = bytes("diagnostics", reg, 2, dynamic);
      return b ? {...b, value:b.values[0] * 256 + b.values[1]} : null;
    }
    function checksum(space, first, last) {
      const b = bytes(space, first, last - first + 1);
      return b ? (b.values.slice(0,-1).reduce((sum,v) => sum+v,0) & 255) === b.values.at(-1) : null;
    }
    const baseIntegrity = checksum("identity",0,63), extIntegrity = checksum("identity",64,95);
    const dmiIntegrity = checksum("diagnostics",0,95);
    function set(id, label, text, tone = "offline", time = null, value = null) {
      result.rows[id] = {label, text:text ?? "Нет данных", tone:text == null ? "unknown" : tone, time, value};
    }
    function ascii(id, label, reg, count) {
      const b = bytes("identity", reg, count);
      const text = b?.values.every(v => v === 0) ? "Не указано" : b && b.values.every(v => v >= 32 && v <= 126)
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
    function enumeration(id, label, reg, values) {
      const e = entry("identity", reg);
      set(id, label, e ? values[e.val] || "Код " + hex(e.val) : null, "offline", e?.updated_ms);
    }
    enumeration("connector", "Разъём", 2, {0:"Не указан",1:"SC",2:"FC style 1",3:"FC style 2",4:"BNC/TNC",
      5:"FC coax",6:"Fiber Jack",7:"LC",8:"MT-RJ",9:"MU",10:"SG",11:"Оптический пигтейл",
      12:"MPO 1×12",13:"MPO 2×16",32:"HSSDC II",33:"Медный пигтейл",34:"RJ45",35:"Неразъёмное соединение",36:"MXC 2×16"});
    enumeration("encoding", "Кодирование", 11, {0:"Не указано",1:"8B/10B",2:"4B/5B",3:"NRZ",
      4:"Manchester",5:"SONET scrambled",6:"64B/66B",7:"256B/257B",8:"PAM4"});
    enumeration("standard", "Редакция SFF-8472", 94, {0:"Не указана",1:"9.3",2:"9.5",3:"10.2",4:"10.4",
      5:"11.0",6:"11.3",7:"11.4",8:"12.3",9:"12.4",10:"12.5"});
    enumeration("rate_select", "Назначение RATE_SELECT", 13, {0:"Не указано",1:"SFF-8079",2:"RX 8/4/2G",
      4:"TX 8/4/2G",6:"RX и TX 8/4/2G",8:"RX 16/8/4G",10:"RX и TX 16/8/4G",12:"RX и TX 32/16/8G",
      14:"CDR 10/8G",16:"64/32/16G",32:"Выбор PMD"});
    set("identifier", "Форм-фактор", "SFP, идентификатор " + hex(identifier));
    const cable = byte("identity", 8), copper = cable == null ? null : !!(cable & 12);
    set("medium", "Среда передачи", copper == null ? null : !copper ? "Оптическая"
      : cable & 4 && cable & 8 ? "Пассивный и активный кабель заявлены" : cable & 4 ? "Пассивный кабель" : "Активный кабель");
    const wavelength = bytes("identity", 60, 2);
    set("wavelength", "Длина волны", copper == null ? null : copper ? "Для кабеля не применяется"
      : wavelength ? wavelength.values[0] || wavelength.values[1]
        ? (wavelength.values[0] * 256 + wavelength.values[1]) + " нм" : "Не указана" : null);
    const compliance = [];
    for (const [r, names] of [[3,["IB 1X copper passive","IB 1X copper active","IB 1X LX","IB 1X SX","10GBASE-SR","10GBASE-LR","10GBASE-LRM","10GBASE-ER"]],
      [6,["1000BASE-SX","1000BASE-LX","1000BASE-CX","1000BASE-T","100BASE-LX/LX10","100BASE-FX","BASE-BX10","BASE-PX"]],
      [9,["FC SM",null,"FC MM 50 мкм","FC MM 62,5 мкм","FC video coax","FC mini coax","FC twisted pair","FC twinax"]],
      [10,["FC 100 MB/s","FC extended","FC 200 MB/s","FC 3200 MB/s","FC 400 MB/s","FC 1600 MB/s","FC 800 MB/s","FC 1200 MB/s"]]]) {
      const v = byte("identity", r);
      if (v != null) names.forEach((n, i) => {if (n && v & (1 << i)) compliance.push(n);});
    }
    const codes = bytes("identity", 3, 8), extendedCode = byte("identity", 36);
    if (extendedCode) compliance.push("Расширенный код " + hex(extendedCode));
    set("compliance", "Стандарты передачи", codes ? compliance.join(", ") || "См. коды совместимости" : null);
    set("compliance_raw", "Все коды совместимости", codes?.values.map(hex).join(" "));
    for (const [id, label, reg, scale, unit] of [["sm_km","Одномодовое волокно",14,1,"км"],
      ["sm_m","Одномодовое волокно, уточнение",15,100,"м"],["om2","OM2",16,10,"м"],
      ["om1","OM1",17,10,"м"],["om4","OM4",18,10,"м"],["om3","OM3",19,10,"м"]]) {
      const v = byte("identity", reg);
      set(id, label, copper == null || v == null ? null : copper ? "Данные кабеля: " + hex(v)
        : v === 0 ? "Не заявлено" : v === 255 ? "> " + 254 * scale + " " + unit : v * scale + " " + unit);
    }
    const date = bytes("identity", 84, 6), lot = bytes("identity", 90, 2);
    const ds = date && String.fromCharCode(...date.values);
    if (/^\d{6}$/.test(ds || "") && Number(ds.slice(2,4)) >= 1 && Number(ds.slice(2,4)) <= 12 && Number(ds.slice(4)) >= 1 && Number(ds.slice(4)) <= 31)
      set("date", "Дата изготовления", ds.slice(4) + "." + ds.slice(2,4) + ".20" + ds.slice(0,2));
    set("lot", "Партия", lot && lot.values.every(v => v === 0 || v >= 32 && v <= 126)
      ? String.fromCharCode(...lot.values).replace(/\0/g,"").trim() || "Не указана" : null);
    const caps = extIntegrity === false ? null : byte("identity", 92);
    const options = extIntegrity === false ? null : byte("identity", 93), status = byte("diagnostics", 110);
    const ddm = caps == null ? null : !!(caps & 0x40);
    const external = caps != null && !!(caps & 0x10), internal = caps != null && !!(caps & 0x20);
    const addressChange = caps != null && !!(caps & 4);
    const hardware = byte("identity", 65), hardwareHigh = byte("identity", 64);
    const statusEntry = entry("diagnostics", 110);
    result.liveStale = result.stale || result.presence === "unavailable" ||
      Number.isFinite(result.now) && statusEntry && result.now - statusEntry.updated_ms > LIVE_MAX_AGE_MS;
    const calibration = ddm === null ? null : !ddm ? "Не применяется"
      : internal === external ? "Не определена" : external ? "Внешняя" : "Внутренняя";
    set("ddm", "Цифровая диагностика DDM", ddm == null ? null : ddm ? "Поддерживается" : "Не поддерживается");
    set("calibration", "Калибровка", calibration, internal === external && ddm ? "warn" : "offline");
    set("rx_type", "Измерение мощности RX", ddm ? caps & 8 ? "Средняя мощность" : "OMA" : null);
    const reason = ddm === null ? "Нет данных о поддержке DDM" : !ddm ? "DDM не поддерживается"
      : addressChange ? "Требуется неподдерживаемое переключение адреса"
      : internal === external ? "Не определён способ калибровки"
      : external && dmiIntegrity === false ? "Ошибка суммы коэффициентов DDM"
      : status == null ? "Нет данных о готовности DDM" : status & 1 ? "Модуль готовит данные DDM" : null;
    set("ready", "Данные DDM", ddm && !addressChange && status != null ? status & 1 ? "Не готовы" : "Готовы" : null,
      status & 1 ? "warn" : "ok", entry("diagnostics", 110)?.updated_ms);
    const state = (id, label, option, mask, zero, one, tone) => {
      const supported = options == null ? null : !!(options & option);
      set(id, label, !ddm || addressChange || supported == null ? null : !supported ? "Не поддерживается"
        : status == null ? null : status & mask ? one : zero,
      supported && status != null && status & mask ? tone : "offline", entry("diagnostics", 110)?.updated_ms);
    };
    state("los", "Приём сигнала RX_LOS", 0x10, 2, "Сигнал обнаружен", "Потеря сигнала", "warn");
    if (options & 0x10 && status != null && ddm && !addressChange) {
      if (hardware == null || !(hardware & 6) || (hardware & 6) === 6)
        set("los", "Приём сигнала RX_LOS", "Полярность не определена", "unknown", statusEntry?.updated_ms);
      else if (hardware & 4) set("los", "Приём сигнала RX_LOS", status & 2 ? "Сигнал обнаружен" : "Потеря сигнала",
        status & 2 ? "ok" : "warn", statusEntry?.updated_ms);
      else if (!(status & 2)) result.rows.los.tone = "ok";
    }
    state("fault", "Ошибка передатчика TX_FAULT", 0x20, 4, "Нет", "Есть", "bad");
    if (result.rows.fault.text === "Нет") result.rows.fault.tone = "ok";
    state("disable", "Программный TX_DISABLE", 0x40, 0x40, "Снят", "Установлен", "warn");
    const hardwareState = (id, label, supported, mask, yes, no) => set(id, label,
      supported == null || !ddm || addressChange ? null : !supported ? "Не поддерживается"
      : status == null ? null : status & mask ? yes : no, "offline", statusEntry?.updated_ms);
    hardwareState("tx_disable", "Вход TX_DISABLE", hardware == null ? null : hardware & 16, 128, "Установлен", "Снят");
    hardwareState("rs0", "Вход RATE_SELECT 0", hardware == null ? null : hardware & 32, 16, "1", "0");
    hardwareState("rs1", "Вход RATE_SELECT 1", hardware == null ? null : hardware & 32, 32, "1", "0");
    for (const [id, label, v, mask] of [["alarm_support","Флаги аварий и предупреждений",options,128],
      ["soft_tx","Программное отключение TX",options,64],["soft_rs0","Программный RATE_SELECT 0",options,8],
      ["soft_rs1","Программный RATE_SELECT 1",options,2],["paging","Дополнительные страницы",hardwareHigh,16],
      ["cdr_support","CDR или ретаймер",hardwareHigh,8],["cooled","Охлаждаемый лазер",hardwareHigh,4],
      ["tunable","Перестраиваемый лазер",hardware,64],["linear","Линейный выход RX",hardwareHigh,1]])
      set(id, label, v == null ? null : v & mask ? "Заявлено" : "Не заявлено");
    set("power_class", "Класс мощности", hardwareHigh == null ? null : hardwareHigh & 64 ? "Класс 4"
      : hardwareHigh & 32 ? "Класс 3" : hardwareHigh & 2 ? "Класс 2" : "Класс 1");

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
      result.rows[metric.id].raw = w?.value;
      result.rows[metric.id].register = "diagnostics:" + hex(metric.reg) + "–" + hex(metric.reg + 1);
      result.rows[metric.id].formula = external ? metric.id === "rx_power"
        ? "Полином RX_PWR(4…0), затем × 0,0001 мВт" : "(ADC × slope / 256 + signed offset) × " + metric.scale + " " + metric.unit
        : (metric.signed ? "signed16" : "uint16") + " × " + metric.scale + " " + metric.unit;
      for (const [j, level] of ["Авария выше", "Авария ниже", "Предупреждение выше", "Предупреждение ниже"].entries()) {
        const threshold = word(i * 8 + j * 2);
        const t = ddm && !addressChange && internal !== external && dmiIntegrity !== false && threshold
          ? calibrated(metric, threshold.value) : null;
        set(metric.id + "_limit" + j, level, t == null ? null : formatted(metric, t), "offline", threshold?.time, t);
      }
      const limits = [0,1,2,3].map(j => result.rows[metric.id + "_limit" + j].value);
      const usable = limits.every(Number.isFinite) && limits[1] < limits[0] && limits[1] <= limits[3] && limits[3] <= limits[2] && limits[2] <= limits[0];
      const row = result.rows[metric.id];
      row.stale = !!(result.liveStale || Number.isFinite(result.now) && Number.isFinite(w?.time) && result.now - w.time > LIVE_MAX_AGE_MS);
      row.thresholds = usable ? limits : null;
      row.level = value == null ? "Нет измерения" : !usable ? "Пороги не определены"
        : value >= limits[0] || value <= limits[1] ? "Аварийный уровень"
        : value >= limits[2] || value <= limits[3] ? "Предупреждение" : "В пределах порогов";
      row.tone = value == null || row.stale ? "unknown" : !usable ? "offline"
        : row.level === "Аварийный уровень" ? "bad" : row.level === "Предупреждение" ? "warn" : "ok";
    }
    const dmi = bytes("diagnostics", 0, 96);
    const dmiMatches = dmi && (dmi.values.slice(0,95).reduce((sum, v) => sum + v, 0) & 255) === dmi.values[95];
    set("dmi_crc", "Сумма диагностики CC_DMI", !ddm || addressChange ? "Не применяется" : dmi ? dmiMatches
      ? "Совпадает" : "Не совпадает с текущими байтами" : null, dmiMatches ? "ok" : "offline", dmi?.time);
    const cdr = byte("diagnostics", 119);
    set("cdr", "Флаги потери захвата CDR", !ddm || addressChange || hardwareHigh == null ? null : !(hardwareHigh & 8)
      ? "Не заявлен" : cdr == null ? null : cdr & 3 ? [cdr & 1 ? "RX" : "",cdr & 2 ? "TX" : ""].filter(Boolean).join(", ") : "Сняты, возможен bypass",
      cdr & 3 ? "warn" : "offline", entry("diagnostics",119)?.updated_ms);
    for (const [id, label, reg, count] of [["laser_raw","Лазер, исходное слово",106,2],
      ["tec_raw","TEC, исходное слово",108,2],["vendor_raw","Область изготовителя",120,7]]) {
      const b = ddm && !addressChange ? bytes("diagnostics",reg,count,true) : null;
      set(id,label,b?.values.map(hex).join(" "),"offline",b?.time);
    }
    for (const [id, label, reg] of [["equalizer","Настройка TX EQ",114],["emphasis","Настройка RX emphasis",115],
      ["extended_status","Расширенное состояние",118],["page","Выбранная страница",127]]) {
      const e = ddm && !addressChange ? entry("diagnostics",reg) : null;
      set(id,label,e ? hex(e.val) : null,"offline",e?.updated_ms);
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
    if (baseIntegrity === false || extIntegrity === false || ddm && dmiIntegrity === false) {
      result.message = "Контрольная сумма не совпадает. Зависимые поля скрыты до корректного чтения.";
      const hide = ids => ids.forEach(id => {
        if (result.rows[id]) Object.assign(result.rows[id],{text:"Ошибка контрольной суммы",value:null,tone:"bad"});
      });
      if (baseIntegrity === false) hide(["vendor","part","revision","oui","rate","connector","encoding","rate_select",
        "identifier","medium","wavelength","compliance","compliance_raw","sm_km","sm_m","om1","om2","om3","om4"]);
      if (extIntegrity === false) hide(["serial","date","lot","standard","power_class","alarm_support","soft_tx",
        "soft_rs0","soft_rs1","paging","cdr_support","cooled","tunable","linear"]);
    }
    if (snapshot.device?.presence === "unavailable") result.message = "Устройство не готово. Показаны ранее прочитанные данные.";
    else if (result.liveStale) result.message = "Измерения устарели. Ожидается обновление кэша.";
    if (result.stale) result.message = "Связь с bvstk потеряна. Показан последний полученный снимок.";
    return result;
  }

  window.bvstkView = {
    apiVersion:1,
    decode,
    mount(root) {
      const style = document.createElement("style");
      style.textContent = `body{margin:0;color:#202c36;font:13px system-ui,sans-serif;line-height:1.45}
        *{box-sizing:border-box}button,select{font:inherit;color:inherit}button{cursor:pointer}
        button:focus-visible,summary:focus-visible,select:focus-visible{outline:2px solid #236292;outline-offset:2px}
        h2{font-size:15px;font-weight:600;margin:0}h3{font-size:13px;margin:0 0 10px}
        .subtitle,.notice,.foot,.label,.time{color:#5f7180}.subtitle{margin:4px 0 16px}
        .notice{min-height:34px;font-size:12px;margin:8px 0 14px}.foot{font-size:12px;margin:10px 0 0;line-height:1.6}
        .health{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));border:1px solid #cad0d5;background:#f5f7f8}
        .health .cell{padding:12px 14px;border-right:1px solid #cad0d5}.health .cell:last-child{border:0}
        .label{display:block;font-size:12px}.value{overflow-wrap:anywhere}span.value{display:block}.health .value{font-size:13px;margin-top:5px}
        .health .value:before{content:'';display:inline-block;width:7px;height:7px;margin-right:7px;background:#9ba5ad;border-radius:1px}
        .health .ok:before{background:#258450}.health .bad:before{background:#b33732}.health .warn:before{background:#b78320}
        .ok{color:#237546}.bad{color:#b33732}.warn{color:#996a16}.unknown{color:#75828d}
        .health .unknown{color:#202c36}.section-head{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:20px 0 10px}
        .metric-grid{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:10px}
        .metric{display:flex;flex-direction:column;text-align:left;background:white;border:1px solid #cad0d5;border-radius:2px;padding:13px 12px;min-width:0;min-height:174px}
        .metric:hover{border-color:#7798af}.metric[aria-expanded=true]{border-color:#236292;background:#f4f8fb}
        .metric .value{font-size:23px;font-weight:600;letter-spacing:-.5px;margin-top:10px;white-space:normal}
        .metric .value.empty{font-size:14px;letter-spacing:0}.secondary{font-size:12px;min-height:18px;color:#5f7180}
        .bar{position:relative;height:5px;background:#e4e8eb;margin:14px 0 8px;border-radius:1px}
        .bar i{position:absolute;top:-4px;height:13px;width:2px;background:#202c36;display:none}
        .level{font-size:11px;min-height:17px}.time{font-size:11px;margin-top:auto;padding-top:6px;font-variant-numeric:tabular-nums}
        .panel{border:1px solid #cad0d5;border-radius:2px;padding:14px 16px;margin-top:14px;min-width:0}
        .panel>h2{padding-bottom:11px;border-bottom:1px solid #e0e5e9;margin-bottom:4px}
        .columns{display:grid;grid-template-columns:1fr 1fr;gap:14px}.columns .panel{margin-top:14px}
        .row{display:grid;grid-template-columns:minmax(130px,44%) minmax(0,1fr);gap:12px;padding:9px 0;border-bottom:1px solid #edf0f2;align-items:start}
        .row:last-child{border:0}.row .value{font-size:13px}.row .label{padding-top:1px}
        .alert-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}.alert-grid .cell{padding:11px 0 4px}.alert-grid .value{margin-top:5px}
        .inspector{background:#f5f8fa}.inspector-head{display:flex;justify-content:space-between;align-items:center}
        .close{background:transparent;border:1px solid #cad0d5;padding:4px 10px;border-radius:2px}
        .limits{overflow-x:auto}table{width:100%;border-collapse:collapse;font-size:12px;white-space:nowrap}
        th,td{text-align:right;padding:11px 10px;border-bottom:1px solid #e1e6ea;font-variant-numeric:tabular-nums}
        th:first-child,td:first-child{text-align:left}thead th{font-weight:500;color:#5f7180;background:#f5f7f8}
        td.current{background:#f2f7fb;font-weight:600}.thresholds .foot{padding:0 2px}
        details.panel{padding:0}details>summary{cursor:pointer;padding:14px 16px;font-weight:600;list-style-position:inside}
        details[open]>summary{border-bottom:1px solid #e0e5e9}details>.body{padding:4px 16px 16px}
        .tools{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin:12px 0}.tools label{display:flex;align-items:center;gap:8px}
        select{background:white;border:1px solid #cad0d5;padding:5px 8px;border-radius:2px}
        svg{display:block;width:100%;height:160px;border-bottom:1px solid #cad0d5;background:repeating-linear-gradient(#fff,#fff 39px,#e8edf0 40px)}
        .graph-labels{display:flex;justify-content:space-between;color:#5f7180;font-size:11px;margin-top:7px}
        [hidden]{display:none!important}@media(max-width:900px){.metric-grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
        @media(max-width:600px){.health{grid-template-columns:1fr 1fr}.health .cell{border-bottom:1px solid #cad0d5}
          .columns,.alert-grid{grid-template-columns:1fr}.metric-grid{grid-template-columns:1fr 1fr}.metric{min-height:166px}
          .health .cell:nth-child(2){border-right:0}.row{grid-template-columns:42% 1fr}.panel{padding:12px}}
        @media(max-width:350px){.metric-grid{grid-template-columns:1fr}.row{grid-template-columns:1fr;gap:3px}}`;
      root.append(style);
      const title = document.createElement("h2"); title.textContent = "Оптический модуль SFP"; root.append(title);
      const subtitle = document.createElement("p"); subtitle.className = "subtitle";
      subtitle.textContent = "Паспорт и цифровая диагностика по данным кэша"; root.append(subtitle);
      this.cells = new Map(); this.cards = new Map(); this.currentCells = new Map();
      this.history = new Map(metrics.map(m => [m.id, []])); this.lastTimes = new Map();
      this.lastNow = null; this.selectedMetric = null; this.data = null;
      const cell = (parent,id,label,cls = "cell") => {
        const node = document.createElement("div"); node.className = cls; node.dataset.metric = id;
        const name = document.createElement("span"); name.className = "label"; name.textContent = label;
        const value = document.createElement("span"); value.className = "value unknown"; value.textContent = "Нет данных";
        node.append(name,value); parent.append(node); this.cells.set(id,{value}); return node;
      };
      const panel = (parent,title,fold = false) => {
        const p = document.createElement(fold ? "details" : "section"); p.className = "panel";
        const h = document.createElement(fold ? "summary" : "h2"); h.textContent = title; p.append(h);
        const body = document.createElement("div"); body.className = "body"; p.append(body); parent.append(p); return body;
      };
      const rows = (parent,defs) => defs.forEach(([id,label]) => cell(parent,id,label,"row"));
      const health = document.createElement("div"); health.className = "health"; root.append(health);
      rows(health,[["presence","Доступ к модулю"],["ready","Данные DDM"],["los","Приём сигнала"],["fault","Ошибка передатчика"]]);
      health.querySelectorAll(".row").forEach(n => n.className = "cell");
      this.notice = document.createElement("p"); this.notice.className = "notice"; root.append(this.notice);
      const heading = document.createElement("div"); heading.className = "section-head";
      heading.innerHTML = '<h2>Текущие измерения</h2><span class="label">Нажмите на показатель для деталей</span>'; root.append(heading);
      const grid = document.createElement("div"); grid.className = "metric-grid"; root.append(grid);
      for (const id of ["rx_power","tx_power","temperature","voltage","bias"]) {
        const m = metrics.find(m => m.id === id), button = document.createElement("button");
        button.type = "button"; button.className = "metric"; button.dataset.metric = id;
        button.setAttribute("aria-expanded","false"); button.setAttribute("aria-controls","metric-inspector");
        button.innerHTML = '<span class="label"></span><span class="value empty unknown">Нет данных</span><span class="secondary"></span><span class="bar"><i></i></span><span class="level"></span><span class="time"></span>';
        button.querySelector(".label").textContent = m.label;
        button.addEventListener("click",() => {
          this.selectedMetric = this.selectedMetric === id ? null : id;
          this.paintInspector();
        });
        grid.append(button);
        this.cards.set(id,{button,value:button.querySelector(".value"),secondary:button.querySelector(".secondary"),
          bar:button.querySelector(".bar"),marker:button.querySelector("i"),level:button.querySelector(".level"),time:button.querySelector(".time")});
      }
      this.inspector = document.createElement("section"); this.inspector.className = "panel inspector";
      this.inspector.id = "metric-inspector"; this.inspector.hidden = true;
      this.inspector.innerHTML = '<div class="inspector-head"><h3></h3><button type="button" class="close">Закрыть</button></div><div class="body"></div>';
      this.inspector.querySelector("button").addEventListener("click",() => {
        const id = this.selectedMetric; this.selectedMetric = null; this.paintInspector(); this.cards.get(id)?.button.focus({preventScroll:true});
      });
      this.inspectCells = new Map();
      for (const label of ["Значение","Регистры","Исходное слово","Преобразование","Метка времени","Состояние данных"]) {
        const n = document.createElement("div"); n.className = "row";
        const name = document.createElement("span"),value = document.createElement("span");
        name.className = "label"; name.textContent = label; value.className = "value"; n.append(name,value);
        this.inspector.querySelector(".body").append(n); this.inspectCells.set(label,value);
      }
      root.append(this.inspector);
      const alerts = panel(root,"Сигналы и предупреждения");
      alerts.classList.add("alert-grid");
      rows(alerts,[["alarms","Аварийные флаги модуля"],["warnings","Предупреждающие флаги модуля"],
        ["tx_disable","Вход TX_DISABLE"],["disable","Программный TX_DISABLE"]]);
      const alertFoot = document.createElement("p"); alertFoot.className = "foot";
      alertFoot.textContent = "Флаги выдаёт модуль; они могут сохраняться до повторного чтения. Цвет измерений показывает сравнение с порогами."; alerts.parentNode.append(alertFoot);
      const columns = document.createElement("div"); columns.className = "columns"; root.append(columns);
      rows(panel(columns,"Паспорт модуля"),[["vendor","Изготовитель"],["part","Модель"],["serial","Серийный номер"],
        ["revision","Ревизия"],["date","Дата изготовления"],["lot","Партия"],["oui","OUI изготовителя"],["identifier","Форм-фактор"]]);
      rows(panel(columns,"Линия и оптика"),[["connector","Разъём"],["medium","Среда передачи"],["wavelength","Длина волны"],
        ["rate","Номинальная символьная скорость"],["encoding","Кодирование"],["compliance","Стандарты передачи"],
        ["rx_type","Измерение мощности RX"],["power_class","Класс мощности"]]);
      const wrap = panel(root,"Пороги изготовителя"); wrap.className = "body thresholds";
      const scroll = document.createElement("div"); scroll.className = "limits"; wrap.append(scroll);
      const table = document.createElement("table"), head = document.createElement("tr");
      for (const label of ["Параметр","Нижняя авария","Нижнее предупреждение","Сейчас","Верхнее предупреждение","Верхняя авария"]) {
        const th = document.createElement("th"); th.textContent = label; head.append(th);
      }
      const thead = document.createElement("thead"); thead.append(head); table.append(thead);
      const tbody = document.createElement("tbody"); table.append(tbody);
      for (const m of metrics) {
        const tr = document.createElement("tr"), name = document.createElement("th");
        name.textContent = m.label; tr.append(name);
        for (const i of [1,3,-1,2,0]) {
          const value = document.createElement("td"); value.textContent = "Нет данных"; tr.append(value);
          if (i < 0) {value.className = "current"; this.currentCells.set(m.id,value);}
          else this.cells.set(m.id + "_limit" + i, {value,limit:true,metric:m});
        }
        tbody.append(tr);
      }
      scroll.append(table);
      const note = document.createElement("p"); note.className = "foot";
      note.textContent = "Пороги берутся из памяти модуля. Оптическая мощность приведена в мВт; dBm доступен на карточках. Нулевые или противоречивые пороги не используются для оценки."; wrap.append(note);
      const caps = panel(root,"Возможности и дальность",true);
      rows(caps,[["ddm","Цифровая диагностика DDM"],["calibration","Калибровка"],["standard","Редакция SFF-8472"],
        ["alarm_support","Флаги аварий"],["soft_tx","Программный TX_DISABLE"],["rate_select","RATE_SELECT"],
        ["soft_rs0","Программный RATE_SELECT 0"],["soft_rs1","Программный RATE_SELECT 1"],["paging","Дополнительные страницы"],
        ["cdr_support","CDR или ретаймер"],["cooled","Охлаждаемый лазер"],["tunable","Перестраиваемый лазер"],
        ["linear","Линейный выход RX"],["sm_km","Одномодовое волокно"],["sm_m","Одномодовое волокно, уточнение"],
        ["om1","OM1"],["om2","OM2"],["om3","OM3"],["om4","OM4"],["compliance_raw","Все коды совместимости"]]);
      const ext = panel(root,"Расширенная диагностика",true);
      rows(ext,[["rs0","Вход RATE_SELECT 0"],["rs1","Вход RATE_SELECT 1"],["cdr","Потеря захвата CDR"],
        ["laser_raw","Лазер, исходное слово"],["tec_raw","TEC, исходное слово"],["equalizer","TX EQ"],
        ["emphasis","RX emphasis"],["extended_status","Расширенное состояние"],["page","Выбранная страница"],["vendor_raw","Область изготовителя"]]);
      const extFoot = document.createElement("p"); extFoot.className = "foot";
      extFoot.textContent = "Назначение слов лазера и TEC, поддержка EQ и emphasis зависят от исполнения. Здесь показаны исходные коды. Для физической расшифровки нужна спецификация изготовителя. Дополнительные страницы не переключаются."; ext.append(extFoot);
      const history = panel(root,"История измерений",true); this.historyPanel = history.parentNode;
      history.innerHTML = '<div class="tools"><label>Показатель <select data-history-metric></select></label><label>Период <select data-history-window><option value="60">1 минута</option><option value="300" selected>5 минут</option><option value="900">15 минут</option></select></label></div><svg viewBox="0 0 1000 160" preserveAspectRatio="none" role="img" aria-label="История измерений"><path fill="none" stroke="#236292" stroke-width="2" vector-effect="non-scaling-stroke"/></svg><div class="graph-labels"><span data-history-range></span><span data-history-time></span></div><p class="foot">История накапливается, пока открыта вкладка. Сохраняются только новые подтверждённые измерения. Разрывы означают отсутствие данных. Мощность отображается в мВт.</p>';
      this.historyMetric = history.querySelector("[data-history-metric]");
      for (const m of metrics) {const o = document.createElement("option"); o.value = m.id; o.textContent = m.label; this.historyMetric.append(o);}
      this.historyWindow = history.querySelector("[data-history-window]");
      this.historyPath = history.querySelector("path"); this.historyRange = history.querySelector("[data-history-range]");
      this.historyTime = history.querySelector("[data-history-time]");
      this.historyMetric.addEventListener("change",() => this.paintHistory()); this.historyWindow.addEventListener("change",() => this.paintHistory());
      this.historyPanel.addEventListener("toggle",() => this.paintHistory());
      const quality = panel(root,"Полнота и достоверность данных",true);
      rows(quality,[["coverage","Заполнение кэша"],["base_crc","CC_BASE"],["ext_crc","CC_EXT"],["dmi_crc","CC_DMI"]]);
      const foot = document.createElement("p"); foot.className = "foot";
      foot.textContent = "Метки времени отсчитываются от старта bvstk. Байты измерения должны иметь одинаковую метку; весь модуль не образует единый мгновенный снимок. Через 5 с без обновления измерения помечаются устаревшими. CC_DMI задаётся при старте модуля и может отличаться после изменения управляющих байтов. RX_LOS показывает оптический сигнал; Ethernet link и согласованная скорость порта здесь не определяются.";
      quality.append(foot);
    },
    stamp(ms) {
      if (!Number.isFinite(ms)) return "—";
      const seconds = Math.floor(ms / 1000);
      return [Math.floor(seconds/3600),Math.floor(seconds/60)%60,seconds%60].map(n => String(n).padStart(2,"0")).join(":") + "." + String(ms%1000).padStart(3,"0");
    },
    paintInspector() {
      const id = this.selectedMetric, row = this.data?.rows[id];
      this.inspector.hidden = !id;
      for (const [key, card] of this.cards) card.button.setAttribute("aria-expanded",String(key === id));
      if (!id) return;
      this.inspector.querySelector("h3").textContent = metrics.find(m => m.id === id).label;
      for (const [label,value] of [["Значение",row?.text],["Регистры",row?.register],["Исходное слово",Number.isInteger(row?.raw) ? "0x" + row.raw.toString(16).toUpperCase().padStart(4,"0") : null],
        ["Преобразование",row?.formula],["Метка времени",this.stamp(row?.time)],
        ["Состояние данных",row?.stale ? "Устарели" : row?.value == null ? "Нет измерения" : row.level]])
        this.inspectCells.get(label).textContent = value ?? "Нет данных";
    },
    paintHistory() {
      if (!this.historyPanel.open) return;
      const id = this.historyMetric.value, metric = metrics.find(m => m.id === id), all = this.history.get(id);
      const end = Math.max(this.data?.now || 0,all.at(-1)?.time || 0), span = Number(this.historyWindow.value)*1000;
      const points = all.filter(p => p.time >= end - span), values = points.map(p => p.value).filter(Number.isFinite);
      if (values.length < 2) {this.historyPath.setAttribute("d","");this.historyRange.textContent = "Ожидаются измерения";this.historyTime.textContent = "";return;}
      const low = Math.min(...values),high = Math.max(...values),pad = Math.max((high-low)*.1,Math.abs(high)*.01,.001);
      let path = "", last = null;
      for (const p of points) {
        if (!Number.isFinite(p.value)) {last = null;continue;}
        const x = Math.max(0,1000*(p.time-(end-span))/span), y = 150-140*(p.value-low+pad)/(high-low+2*pad);
        path += (last != null && p.time-last <= LIVE_MAX_AGE_MS ? "L" : "M") + x.toFixed(1)+","+y.toFixed(1);
        last = p.time;
      }
      this.historyPath.setAttribute("d",path);
      this.historyRange.textContent = low.toFixed(metric.digits) + " … " + high.toFixed(metric.digits) + " " + metric.unit;
      this.historyTime.textContent = this.stamp(Math.max(0,end-span)) + " — " + this.stamp(end);
    },
    update(snapshot) {
      const data = decode(snapshot);
      this.data = data;
      if (Number.isFinite(data.now) && Number.isFinite(this.lastNow) && data.now < this.lastNow) {
        this.history.forEach(a => a.splice(0)); this.lastTimes.clear();
      }
      this.lastNow = data.now;
      data.rows.presence = {text:data.presence === "responding" && !data.stale ? "Готов" : "Не готов",
        tone:data.presence === "responding" && !data.stale ? "ok" : "unknown"};
      data.rows.coverage = {text:data.count + " из 256 байт",tone:"offline"};
      this.notice.textContent = data.message + " В кэше " + data.count + " из 256 байт.";
      for (const [id, cell] of this.cells) {
        const row = data.rows[id];
        cell.value.textContent = cell.limit && Number.isFinite(row?.value)
          ? row.value.toFixed(cell.metric.digits) + " " + cell.metric.unit : row?.text || "Нет данных";
        const live = ["ready","los","fault","alarms","warnings","tx_disable","disable","cdr","rs0","rs1",
          "laser_raw","tec_raw","equalizer","emphasis","extended_status"].includes(id);
        cell.value.className = "value " + (data.stale || live && data.liveStale ? "unknown" : row?.tone || "unknown");
        cell.value.title = Number.isFinite(row?.time) ? "Метка времени: " + row.time + " мс" : "";
      }
      for (const [id,c] of this.cards) {
        const r = data.rows[id], metric = metrics.find(m => m.id === id), valid = Number.isFinite(r?.value);
        const power = metric.unit === "мВт";
        c.value.textContent = valid ? power && r.value > 0 ? (10*Math.log10(r.value)).toFixed(2) + " dBm"
          : r.value.toFixed(metric.digits) + " " + metric.unit : "Нет данных";
        c.value.className = "value " + (!valid ? "empty unknown" : r.tone);
        c.secondary.textContent = valid && power && r.value > 0 ? r.value.toFixed(4) + " мВт" : !valid ? r?.text || "Ожидается чтение" : "";
        c.level.textContent = r?.stale ? "Данные устарели" : r?.level || "Нет измерения";
        c.level.className = "level " + (r?.tone || "unknown");
        c.time.textContent = "Метка времени " + this.stamp(r?.time);
        const t = r?.thresholds;
        if (valid && t) {
          const percent = n => Math.min(100,Math.max(0,100*(n-t[1])/(t[0]-t[1])));
          c.bar.style.background = `linear-gradient(to right,#d7a1a0 0% 4%,#e6cf9d 4% ${percent(t[3])}%,#b8d2c2 ${percent(t[3])}% ${percent(t[2])}%,#e6cf9d ${percent(t[2])}% 96%,#d7a1a0 96% 100%)`;
          c.marker.style.left = percent(r.value)+"%"; c.marker.style.display = "block";
        } else {c.bar.style.background = "#e4e8eb";c.marker.style.display = "none";}
        const current = this.currentCells.get(id);
        current.textContent = valid ? r.value.toFixed(metric.digits) + " " + metric.unit : "Нет данных";
        current.className = "current " + (r?.tone || "unknown");
        const history = this.history.get(id);
        if (valid && !r.stale && Number.isFinite(r.time) && this.lastTimes.get(id) !== r.time) {
          if (history.at(-1)?.time > r.time) history.splice(0);
          history.push({time:r.time,value:r.value}); this.lastTimes.set(id,r.time);
        } else if ((!valid || r?.stale) && history.at(-1)?.value != null) {
          history.push({time:data.now ?? history.at(-1).time,value:null});
        }
        while (history.length > 1800 || history.length && history[0].time < (data.now || history.at(-1).time) - 900000) history.shift();
      }
      this.paintInspector(); this.paintHistory();
    }
  };
}());
