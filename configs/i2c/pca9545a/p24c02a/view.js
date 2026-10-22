// P24C02A memory inspector. Physical cache snapshots only; no bus requests.
(() => {
  "use strict";
  const SIZE = 256, PAGE = 8, HISTORY_LIMIT = 200;
  const hex = value => value.toString(16).toUpperCase().padStart(2, "0");
  const addr = value => "0x" + hex(value);
  const printable = value => value >= 32 && value <= 126;
  const plural = (n, one, few, many) => n % 100 >= 11 && n % 100 <= 14 ? many :
    n % 10 === 1 ? one : n % 10 >= 2 && n % 10 <= 4 ? few : many;
  const stamp = ms => {
    if (!Number.isSafeInteger(ms) || ms < 0) return "—";
    const seconds = Math.floor(ms / 1000);
    return [Math.floor(seconds / 3600), Math.floor(seconds / 60) % 60, seconds % 60]
      .map(n => String(n).padStart(2, "0")).join(":") + "." + String(ms % 1000).padStart(3, "0");
  };

  // Retain last observations separately from validity. A missing read is never
  // interpreted as a zero or an EEPROM write; comparison restarts after a gap.
  function createTracker() {
    let identity, clock = null, epoch = 0, lastMark = -1, total = 0, sequence = 0;
    let last = Array(SIZE).fill(null), previous = Array(SIZE).fill(null);
    let history = [], changed = new Set();
    function reset() {
      last = Array(SIZE).fill(null); previous = Array(SIZE).fill(null);
      history = []; changed = new Set(); total = sequence = 0; lastMark = -1; epoch++;
    }
    return {accept(snapshot) {
      const key = String(snapshot.device?.bus || "") + ":" + String(snapshot.device?.id || "");
      const now = Number.isSafeInteger(snapshot.uptime_ms) && snapshot.uptime_ms >= 0 ? snapshot.uptime_ms : null;
      // The HTTP clock is interpolated between responses and can step back by
      // a few milliseconds. Physical observation timestamps give a second
      // lower bound when detecting a restart.
      if (identity !== key || now !== null && clock !== null &&
          (now + 1000 < clock || now < lastMark)) reset();
      identity = key; clock = now;
      const stale = snapshot.stale === true, presence = snapshot.device?.presence || "unknown";
      const available = !stale && presence !== "unavailable";
      const entries = Array(SIZE).fill(null);
      for (const e of snapshot.cache || []) {
        if (Number.isInteger(e.reg) && e.reg >= 0 && e.reg < SIZE &&
            e.valid === true && ["I2C", "read"].includes(e.source) &&
            Number.isInteger(e.val) && e.val >= 0 && e.val <= 255 &&
            Number.isSafeInteger(e.updated_ms) && e.updated_ms >= 0)
          entries[e.reg] = {value:e.val, time:e.updated_ms};
      }
      const additions = [];
      const bytes = entries.map((entry, index) => {
        if (!available || !entry || last[index] && entry.time < last[index].time) {
          previous[index] = null;
          return last[index] ? {...last[index], current:false} : null;
        }
        const before = previous[index];
        if (before && entry.value !== before.value) {
          additions.push({id:++sequence, address:index, before:before.value, after:entry.value, time:entry.time});
          changed.add(index); total++;
        }
        previous[index] = entry; last[index] = entry;
        lastMark = Math.max(lastMark, entry.time);
        return {...entry, current:true};
      });
      history = [...history, ...additions].sort((a, b) => b.time - a.time || b.id - a.id).slice(0, HISTORY_LIMIT);
      return {bytes, history:history.slice(), changed:[...changed], total, epoch, stale, presence,
        count:bytes.filter(e => e?.current).length, lastTime:lastMark >= 0 ? lastMark : null,
        id:snapshot.device?.id || "eeprom", model:snapshot.map?.device || "P24C02A"};
    }};
  }

  let root, latest, shown, tracker, paused = false, selected = 0, historySignature = "", historyEpoch = -1;
  let cells, asciiCells, bitCells;
  const $ = id => root.querySelector("#" + id);
  const svgIcon = (body, name = "") => '<svg class="icon" aria-hidden="true" viewBox="0 0 24 24" ' +
    (name ? 'data-icon="' + name + '"' : '') + '>' + body + '</svg>';
  const icon = (path, name = "") => svgIcon('<path d="' + path + '"/>', name);
  const memoryIcon = () => svgIcon('<rect x="5" y="4" width="14" height="16" rx="1"/><path d="M2 7h3M2 12h3M2 17h3m14-10h3m-3 5h3m-3 5h3M9 8h6m-6 4h6m-6 4h3"/>', "memory");
  const gridIcon = () => svgIcon('<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>', "grid");
  const changeIcon = () => svgIcon('<path d="M4 7h15m-4-4 4 4-4 4M20 17H5m4-4-4 4 4 4"/>', "change");
  const clockIcon = () => svgIcon('<circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/>', "clock");
  const eyeIcon = () => svgIcon('<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/>', "eye");
  const pauseIcon = () => icon("M8 4v16M16 4v16");
  const playIcon = () => icon("m8 4 12 8-12 8Z");
  const style = `
    :root{--ink:#202c36;--muted:#627481;--blue:#236292;--line:#d1d8dd;--amber:#996516;--mono:ui-monospace,SFMono-Regular,Consolas,monospace}
    *{box-sizing:border-box}#device-root{font:13px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:var(--ink)}
    button,input{font:inherit;border-radius:2px}button{cursor:pointer}button:disabled{cursor:default;color:#85929d;background:#f1f3f5;border-color:var(--line)}
    button:focus-visible,input:focus-visible,summary:focus-visible{outline:2px solid #397ba9;outline-offset:2px}[hidden]{display:none!important}
    .mono{font-family:var(--mono);font-variant-numeric:tabular-nums}.icon{width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:1.7;stroke-linecap:round;stroke-linejoin:round;flex:none}
    .btn{min-height:33px;display:inline-flex;align-items:center;justify-content:center;gap:7px;padding:5px 11px;color:var(--blue);border:1px solid #bac8d2;background:white;font-weight:500;font-size:12px}
    .btn:hover:not(:disabled){background:#f0f6fa}.btn[aria-pressed=true]{background:#fff2d6;border-color:#caa157;color:#87570c}
    .overview{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin:0 0 16px}.metric{border:1px solid var(--line);padding:13px 16px;min-height:104px;position:relative;border-radius:3px;background:#fff}
    .metric-label{color:var(--muted);font-size:11px;display:flex;justify-content:space-between;align-items:center}.metric-label .icon{width:16px;height:16px;color:#8c9ba7}.metric-value{font-size:27px;font-weight:600;line-height:1.35;margin-top:5px;letter-spacing:-.8px;font-variant-numeric:tabular-nums}
    .metric-value small{font-size:13px;font-weight:400;letter-spacing:0;color:var(--muted)}.metric .time{font-size:22px}.metric-note{font-size:11px;color:var(--muted);margin-top:3px}
    .coverage{position:absolute;bottom:0;left:0;right:0;height:3px;background:#e7ecef}.coverage span{display:block;height:100%;background:#4c85ad}
    .state-banner{padding:9px 12px;background:#eef3f6;border:1px solid #c4d0d9;color:#506572;font-size:12px;margin:0 0 12px}
    .state-banner.paused{background:#fff6e6;border-color:#e7d2ab;color:#8a611e}
    .workbench{display:grid;grid-template-columns:minmax(0,1fr) 254px;gap:14px;align-items:start}
    .panel{background:#fff;border:1px solid var(--line);border-radius:3px;min-width:0;overflow:hidden}
    .panel-head{min-height:47px;padding:11px 15px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;gap:12px}
    .panel-head h2{margin:0;font-size:14px;font-weight:600;display:flex;align-items:center;gap:8px}.panel-head>.icon{width:17px;height:17px;color:#7c919f}.subtle{font-size:11px;color:var(--muted)}
    .memory-toolbar{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:9px 12px;border-bottom:1px solid #e2e7eb;background:#fbfcfd;flex-wrap:wrap}
    .goto,.tools{display:flex;gap:7px;align-items:center}.goto label{font-size:11px;color:var(--muted)}
    .goto input{width:74px;padding:5px 8px;height:32px;border:1px solid #c4cfd8;font:12px var(--mono);color:var(--ink);background:#fff}.goto .btn{min-height:32px;padding:4px 7px}
    .memory-scroll{overflow:auto;max-width:100%;scrollbar-color:#c3cdd5 #f3f5f7}
    .memory-table{border-collapse:separate;border-spacing:0;table-layout:fixed;width:100%;min-width:600px;font-family:var(--mono);font-size:12px}
    .memory-table .offset-col{width:60px}.memory-table .ascii-col{width:140px}
    .memory-table th{font-size:10px;font-weight:400;color:#718592;background:#f5f7f9;height:31px;border-bottom:1px solid var(--line)}
    .memory-table thead th:first-child{text-align:left;padding-left:12px}
    .memory-table tbody th{text-align:left;padding-left:12px;border-right:1px solid #e0e6ea;font-size:11px;height:30px}
    .memory-table td{padding:0;height:30px;border-bottom:1px solid #edf0f3;position:relative}
    .memory-table tr:last-child td,.memory-table tr:last-child th{border-bottom:0}
    .memory-table .page-start{border-left:2px solid #ccd6de}
    .byte{width:100%;height:30px;display:block;padding:0;border:0;border-radius:0;background:#fff;font:12px var(--mono);color:#354c5c;position:relative}
    .byte:hover{background:#eef4f8}.byte.unknown{color:#9aa8b2;background:repeating-linear-gradient(135deg,#f8fafb,#f8fafb 4px,#f0f4f6 4px,#f0f4f6 5px)}
    .byte.changed{background:#fff2d6;color:#935d10;font-weight:600}.byte.old{color:#7c8d98;background:#edf0f2;font-weight:400}
    .byte.selected{background:#dfeef9;color:#134f7b;box-shadow:inset 0 0 0 2px #367dad;z-index:1}
    .byte.selected.changed::after{content:"";width:5px;height:5px;background:#b88325;position:absolute;top:3px;right:3px}
    .byte:focus-visible{outline:2px solid #184c76;outline-offset:-2px;z-index:2}
    .memory-table td.ascii-cell{padding:0 7px 0 10px;border-left:1px solid var(--line);white-space:nowrap;font-size:10px;color:#71828e}
    .ascii-char{cursor:pointer;display:inline-block;min-width:6.5px;height:23px;line-height:23px;text-align:center}
    .ascii-char.changed{background:#fff2d6;color:#935d10}.ascii-char.old{color:#929da6;background:#edf0f2}.ascii-char.selected{background:#dfeef9;color:#134f7b}
    .memory-legend{display:flex;gap:13px;align-items:center;flex-wrap:wrap;font-size:10px;color:var(--muted);padding:10px 12px;border-top:1px solid var(--line);background:#fbfcfd}
    .memory-legend span{display:inline-flex;align-items:center;gap:6px}.swatch{width:10px;height:10px;background:#fff;border:1px solid #b9c7d2;display:inline-block}
    .swatch.changed{background:#fff2d6;border-color:#d4ad63}.swatch.unknown{background:#e9eff3}.swatch.old{background:#edf0f2}.swatch.selected{background:#dfeef9;border-color:#367dad}
    .memory-caption{padding:8px 12px;font-size:10px;color:#718490;border-top:1px solid #e8edf0}
    .inspector-head{background:#f6f8fa}.inspector-body{padding:16px}.address-line{display:flex;align-items:center;justify-content:space-between;color:var(--muted);font-size:11px}
    .address-chip{font-family:var(--mono);color:#3e5b70;background:#edf2f6;padding:3px 7px;border:1px solid #d7e0e6;border-radius:2px}
    .inspector-value{font:34px/1.3 var(--mono);letter-spacing:-1px;margin:12px 0 2px;color:#204b6b}
    .value-caption{font-size:11px;color:var(--muted);margin-bottom:16px}.value-caption.warn{color:var(--amber)}
    .kv{display:grid;grid-template-columns:1fr auto;gap:10px 12px;margin:0;font-size:12px}.kv dt{color:var(--muted)}.kv dd{margin:0;text-align:right}
    .inspector-section{border-top:1px solid #e1e7eb;padding:13px 16px}.section-label{font-size:11px;color:var(--muted);margin:0 0 9px;font-weight:500}
    .bits{display:grid;grid-template-columns:repeat(8,1fr);gap:3px}.bit{display:flex;flex-direction:column;align-items:center;gap:5px}
    .bit small{font:9px var(--mono);color:#8798a4}.bit span{height:27px;width:100%;text-align:center;font:12px/27px var(--mono);background:#f1f4f6;border:1px solid #dde4e9;color:#7c8d99;border-radius:2px}
    .bit span.on{background:#dfedf7;color:#1b5c89;border-color:#b6d1e3}.last-time{font:14px var(--mono);color:#405c70;margin:0 0 4px}
    .tiny{font-size:10px;color:var(--muted);line-height:1.6}.byte-history{list-style:none;padding:0;margin:7px 0 0;display:grid;gap:9px}
    .byte-history li{font-size:11px;display:flex;align-items:center;justify-content:space-between;gap:8px}.byte-history time{color:var(--muted);font:10px var(--mono)}
    .inspector-foot{background:#f6f8fa;border-top:1px solid var(--line);padding:11px 16px;font-size:10px;color:var(--muted);line-height:1.6}.inspector-foot .icon{display:inline-block;width:12px;height:12px;vertical-align:-2px;margin-right:5px}
    .disclosure{margin-top:14px}.disclosure>summary{display:flex;align-items:center;gap:10px;list-style:none;padding:13px 15px;font-size:13px;font-weight:600;cursor:pointer}
    .disclosure>summary::-webkit-details-marker{display:none}.disclosure>summary::before{content:"›";font-size:18px;color:#71838f;line-height:1}
    .disclosure[open]>summary::before{transform:rotate(90deg)}.disclosure[open]>summary{border-bottom:1px solid var(--line)}
    .disclosure>summary small{font-size:11px;color:var(--muted);font-weight:400;margin-left:auto}
    .change-scroll{max-height:255px}.change-table{width:100%;border-collapse:collapse;font-size:12px;text-align:left}
    .change-table th{font-weight:400;font-size:11px;color:var(--muted);background:#f7f9fa;padding:9px 15px;position:sticky;top:0}
    .change-table td{padding:9px 15px;border-top:1px solid #ebeff2}.value-new{color:var(--amber)}
    .address-button{font:12px var(--mono);padding:0;color:var(--blue);border:0;background:none;text-decoration:underline;text-underline-offset:3px}
    .change-note{padding:10px 15px;margin:0;border-top:1px solid #e4eaee;font-size:11px;color:var(--muted);background:#fbfcfd}
    .empty-log{text-align:center;color:var(--muted);padding:22px!important}
    .specs{padding:16px}.specs .kv{grid-template-columns:1fr 1fr;max-width:600px}.spec-note{padding:0 16px 16px;margin:0;font-size:11px;color:var(--muted);line-height:1.65}
    .feedback{margin:8px 0 0;font-size:11px;color:var(--blue)}.feedback:empty{display:none}
    @media(max-width:1050px){.workbench{grid-template-columns:minmax(0,1fr)}.inspector{display:grid;grid-template-columns:1fr 1fr 1fr}
      .inspector-head{grid-column:1/-1}.inspector-body{grid-row:2/4}.inspector-section{border-top:0;border-left:1px solid var(--line)}
      .inspector-section.byte-updates{grid-column:3;grid-row:2/4}.inspector-foot{grid-column:1/-1}}
    @media(max-width:620px){.overview{grid-template-columns:1fr 1fr;gap:9px}.metric{padding:12px;min-height:94px}.metric-value{font-size:24px}
      .metric:last-child{grid-column:1/-1;min-height:68px;display:grid;grid-template-columns:1fr auto;align-items:center}
      .metric:last-child .metric-value{grid-column:2;grid-row:1/3;margin:0;font-size:18px}.metric:last-child .metric-note{grid-column:1;font-size:10px}
      .metric-label{font-size:10px}.panel-head{padding:11px 12px}.tools{margin-left:auto}.btn{padding:5px 8px;font-size:11px}
      .inspector{grid-template-columns:1fr 1fr}.inspector-body{grid-column:1;grid-row:2/4;padding:12px}
      .inspector-section{padding:12px}.inspector-section.byte-updates{grid-column:1/-1;grid-row:auto;border-top:1px solid var(--line);border-left:0}
      .bits{gap:2px}.bit span{font-size:10px}.last-time{font-size:12px}.kv{font-size:11px;gap:9px}.inspector-value{font-size:30px}
      .memory-legend{gap:10px;font-size:9px}.memory-caption,.change-note{font-size:10px}
      .disclosure>summary{padding:12px;font-size:12px}.disclosure>summary small{font-size:10px}
      .change-table th,.change-table td{padding:9px 11px;white-space:nowrap}.change-table{font-size:11px}}
  `;

  function setText(node, text) {
    text = String(text);
    if (node.textContent !== text) node.textContent = text;
  }
  function select(index, focus = false) {
    if (!shown) return;
    selected = index; $("address-input").value = addr(index); $("address-input").setCustomValidity("");
    renderMemory(); renderInspector();
    if (focus) {
      cells[index].focus({preventScroll:true});
      const cell = cells[index].getBoundingClientRect(), frame = $("memory-scroll").getBoundingClientRect();
      if (cell.left < frame.left) $("memory-scroll").scrollLeft -= frame.left - cell.left + 8;
      if (cell.right > frame.right) $("memory-scroll").scrollLeft += cell.right - frame.right + 8;
    }
  }
  function renderMemory() {
    const changed = new Set(shown.changed);
    for (let i = 0; i < SIZE; i++) {
      const e = shown.bytes[i], cell = cells[i], ascii = asciiCells[i];
      setText(cell, e ? hex(e.value) : "—");
      cell.className = "byte" + (!e ? " unknown" : !e.current ? " old" : changed.has(i) ? " changed" : "") +
        (i === selected ? " selected" : "");
      cell.tabIndex = i === selected ? 0 : -1; cell.setAttribute("aria-pressed", String(i === selected));
      cell.title = addr(i) + ": " + (e ? addr(e.value) + ", " + stamp(e.time) + (!e.current ? ", последнее известное" : "") : "нет данных");
      cell.setAttribute("aria-label", cell.title);
      setText(ascii, e ? printable(e.value) ? String.fromCharCode(e.value) : "·" : "—");
      ascii.className = "ascii-char" + (e && !e.current ? " old" : changed.has(i) && e ? " changed" : "") + (i === selected ? " selected" : "");
      ascii.title = cell.title;
    }
  }
  function renderInspector() {
    const e = shown.bytes[selected], entries = shown.history.filter(c => c.address === selected);
    setText($("selected-address"), addr(selected)); setText($("selected-value"), e ? addr(e.value) : "—");
    setText($("selected-status"), !e ? "Нет данных в кэше" : !e.current ? "Последнее известное значение" :
      shown.changed.includes(selected) ? "Значение изменилось за сеанс" : "Значение в кэше");
    $("selected-status").className = "value-caption" + (e?.current && shown.changed.includes(selected) ? " warn" : "");
    setText($("selected-dec"), e ? e.value : "—");
    setText($("selected-ascii"), !e ? "—" : printable(e.value) ? "«" + String.fromCharCode(e.value) + "»" : "Непечатный");
    setText($("selected-page"), addr(Math.floor(selected / PAGE) * PAGE) + "–" + addr(Math.floor(selected / PAGE) * PAGE + PAGE - 1));
    setText($("selected-time"), e ? stamp(e.time) : "—");
    setText($("selected-time-note"), e ? "От старта системы" : "Ожидание чтения байта");
    for (let i = 0; i < 8; i++) {const bit = e ? (e.value >> i) & 1 : null; setText(bitCells[i], bit ?? "—"); bitCells[i].className = bit === 1 ? "on" : "";}
    $("bits").setAttribute("aria-label", e ? e.value.toString(2).padStart(8, "0") : "Нет данных");
    $("byte-history").replaceChildren(); $("byte-history-empty").hidden = entries.length > 0;
    for (const change of entries.slice(0, 3)) {
      const li = document.createElement("li"), t = document.createElement("time"), value = document.createElement("span");
      t.textContent = stamp(change.time).slice(0, -4); value.className = "mono"; value.textContent = hex(change.before) + " → " + hex(change.after);
      li.append(t, value); $("byte-history").append(li);
    }
  }
  function renderHistory() {
    setText($("history-meta"), shown.total + " " + plural(shown.total, "изменение", "изменения", "изменений") + " за сеанс");
    const key = shown.epoch + ":" + shown.total;
    if (key === historySignature) return;
    historySignature = key;
    const body = $("history-body"), container = $("change-scroll"), top = container.scrollTop;
    if (historyEpoch !== shown.epoch) { body.replaceChildren(); historyEpoch = shown.epoch; }
    const existing = new Map([...body.querySelectorAll("tr[data-event]")].map(n => [Number(n.dataset.event), n]));
    if (shown.history.length) body.querySelector(".empty-row")?.remove();
    const keep = new Set(shown.history.map(e => e.id));
    for (const [id, row] of existing) if (!keep.has(id)) row.remove();
    // Insert by observation time around stable existing rows. A lower-address
    // byte may arrive later than a higher-address byte in the same HTTP poll.
    let anchor = null;
    for (const change of [...shown.history].reverse()) {
      if (existing.has(change.id)) { anchor = existing.get(change.id); continue; }
      const row = document.createElement("tr"); row.dataset.event = change.id;
      for (const [i, value] of [stamp(change.time), addr(change.address), addr(change.before), addr(change.after)].entries()) {
        const td = document.createElement("td"); td.className = "mono" + (i === 3 ? " value-new" : "");
        if (i === 1) {
          const button = document.createElement("button"); button.className = "address-button"; button.textContent = value;
          button.onclick = () => {select(change.address, true); $("memory-title").scrollIntoView({block:"nearest"});};
          td.append(button);
        } else td.textContent = value;
        row.append(td);
      }
      body.insertBefore(row, anchor); anchor = row;
    }
    if (!shown.history.length && !body.children.length) {
      const row = document.createElement("tr"), td = document.createElement("td");
      row.className = "empty-row"; td.className = "empty-log"; td.colSpan = 4; td.textContent = "Изменений не обнаружено";
      row.append(td); body.append(row);
    }
    container.scrollTop = top;
    setText($("history-limit"), shown.total > HISTORY_LIMIT ? "Показаны последние " + HISTORY_LIMIT + " изменений. " : "");
  }
  function render() {
    if (!shown) return;
    setText($("known-count"), shown.count); setText($("changed-count"), shown.changed.length);
    setText($("changed-unit"), plural(shown.changed.length, "байт", "байта", "байт"));
    $("coverage-bar").style.width = shown.count / SIZE * 100 + "%";
    setText($("coverage-note"), shown.count === SIZE ? "Весь диапазон прочитан" :
      shown.stale ? "Ожидание связи с системой" : shown.presence === "unavailable" ? "Последние известные значения" :
      shown.count ? "Осталось прочитать " + (SIZE - shown.count) + " байт" : "Ожидание чтения памяти");
    setText($("last-read"), stamp(shown.lastTime)); setText($("model"), shown.model);
    const banner = $("state-banner");
    const uncertain = shown.stale || shown.presence !== "responding";
    banner.hidden = !paused && !uncertain;
    banner.className = "state-banner" + (paused ? " paused" : "");
    setText(banner, paused ? "Пауза. Память, временные метки и журнал зафиксированы. Нажмите «Продолжить», чтобы увидеть новые данные." :
      shown.stale ? "Связь с bvstk потеряна. Показаны последние известные значения." :
      shown.presence === "unavailable" ? "Устройство не отвечает. Последние известные значения выделены серым; их актуальность не подтверждена." :
      "Готовность устройства ещё не подтверждена. Данные появятся после успешного чтения.");
    $("download").disabled = shown.count !== SIZE || uncertain;
    $("download").title = $("download").disabled ? "Нужны все 256 достоверных байт и подтверждённая связь" : "Скачать показанные 256 байт";
    $("old-legend").hidden = !shown.bytes.some(e => e && !e.current);
    renderMemory(); renderInspector(); renderHistory();
  }
  function mount(target) {
    root = target; tracker = createTracker(); latest = shown = null; paused = false; selected = 0; historySignature = ""; historyEpoch = -1;
    cells = []; asciiCells = []; bitCells = [];
    root.innerHTML = `<style>${style}</style>
      <div class="overview">
        <section class="metric"><div class="metric-label">Данные в кэше${memoryIcon()}</div><div class="metric-value"><span id="known-count">0</span> <small>из 256 байт</small></div><div class="metric-note" id="coverage-note">Ожидание чтения памяти</div><div class="coverage" aria-hidden="true"><span id="coverage-bar"></span></div></section>
        <section class="metric"><div class="metric-label">Изменились за сеанс${changeIcon()}</div><div class="metric-value"><span id="changed-count">0</span> <small id="changed-unit">байт</small></div><div class="metric-note">По наблюдаемым значениям</div></section>
        <section class="metric"><div class="metric-label">Последнее чтение${clockIcon()}</div><div class="metric-value time mono" id="last-read">—</div><div class="metric-note">Метка времени от старта системы</div></section>
      </div>
      <div class="state-banner" id="state-banner" hidden role="status"></div>
      <div class="workbench">
        <section class="panel"><div class="panel-head"><h2 id="memory-title">${gridIcon()}Содержимое памяти</h2><span class="subtle mono">0x00–0xFF</span></div>
          <div class="memory-toolbar"><div class="goto"><label for="address-input">Адрес</label><input id="address-input" value="0x00" maxlength="4" autocomplete="off" spellcheck="false" title="HEX-адрес от 00 до FF"><button class="btn" id="go" aria-label="Перейти к адресу">${icon("M4 12h16m-6-6 6 6-6 6")}</button></div>
            <div class="tools"><button class="btn" id="pause" aria-pressed="false">${pauseIcon()}<span>Пауза</span></button><button class="btn" id="download" disabled>${icon("M12 3v13m-5-5 5 5 5-5M3 16v5h18v-5")}<span>Снимок .bin</span></button></div></div>
          <div class="memory-scroll" id="memory-scroll"><table class="memory-table" aria-label="256 байт EEPROM. Стрелки перемещают выбор."><colgroup><col class="offset-col"><col span="16"><col class="ascii-col"></colgroup><thead><tr id="memory-header"><th scope="col">Адрес</th></tr></thead><tbody id="memory-body"></tbody></table></div>
          <div class="memory-legend"><span><i class="swatch"></i>Прочитан</span><span><i class="swatch changed"></i>Изменился</span><span><i class="swatch unknown"></i>Нет данных</span><span><i class="swatch selected"></i>Выбран</span><span id="old-legend" hidden><i class="swatch old"></i>Последнее известное</span></div>
          <div class="memory-caption">16 байт в строке. Граница страницы через 8 байт. Перемещение по ячейкам: ← ↑ ↓ →</div>
        </section>
        <aside class="panel inspector">
          <div class="panel-head inspector-head"><h2>Выбранный байт</h2>${eyeIcon()}</div>
          <div class="inspector-body"><div class="address-line"><span>Адрес</span><span class="address-chip" id="selected-address">0x00</span></div><div class="inspector-value" id="selected-value">—</div><div class="value-caption" id="selected-status">Нет данных в кэше</div><dl class="kv"><dt>Десятичное</dt><dd class="mono" id="selected-dec">—</dd><dt>Символ ASCII</dt><dd class="mono" id="selected-ascii">—</dd><dt>Страница</dt><dd class="mono" id="selected-page">0x00–0x07</dd></dl></div>
          <section class="inspector-section"><h3 class="section-label">Биты</h3><div class="bits" id="bits" role="img" aria-label="Нет данных"></div></section>
          <section class="inspector-section"><h3 class="section-label">Последнее обновление</h3><p class="last-time" id="selected-time">—</p><div class="tiny" id="selected-time-note">От старта системы</div></section>
          <section class="inspector-section byte-updates"><h3 class="section-label">Последние изменения байта</h3><ul class="byte-history" id="byte-history"></ul><p class="tiny" id="byte-history-empty">Изменений не обнаружено</p></section>
          <div class="inspector-foot">${eyeIcon()}Просмотр содержимого кэша. Значения обновляются после взаимодействия с устройством.</div>
        </aside>
      </div>
      <p class="feedback" id="feedback" role="status"></p>
      <details class="panel disclosure" open id="history-panel"><summary>Изменения содержимого<small id="history-meta">0 изменений за сеанс</small></summary><div class="memory-scroll change-scroll" id="change-scroll"><table class="change-table"><thead><tr><th>Метка времени</th><th>Адрес</th><th>Было</th><th>Стало</th></tr></thead><tbody id="history-body"></tbody></table></div><p class="change-note"><span id="history-limit"></span>История текущей вкладки. Изменения между двумя наблюдениями могут быть пропущены. Первое значение после разрыва связи служит началом нового сравнения.</p></details>
      <details class="panel disclosure"><summary>Сведения о памяти<small id="model">P24C02A</small></summary><div class="specs"><dl class="kv"><dt>Ёмкость</dt><dd>256 байт</dd><dt>Диапазон адресов</dt><dd class="mono">0x00–0xFF</dd><dt>Страница записи</dt><dd>8 байт</dd><dt>Изготовитель по карте</dt><dd>Puya</dd></dl></div><p class="spec-note">Формат содержимого не описан. Для расшифровки серийного номера и других полей потребуется описание их расположения. Значения 00 и FF сами по себе не обозначают свободную область. Снимок объединяет чтения, выполненные в разное время.</p></details>`;
    for (let c = 0; c < 16; c++) {
      const th = document.createElement("th"); th.scope = "col"; th.textContent = hex(c);
      if (c === 8) th.className = "page-start"; $("memory-header").append(th);
    }
    const th = document.createElement("th"); th.scope = "col"; th.textContent = "Текст ASCII"; $("memory-header").append(th);
    for (let row = 0; row < 16; row++) {
      const tr = document.createElement("tr"), offset = document.createElement("th");
      offset.scope = "row"; offset.textContent = addr(row * 16); tr.append(offset);
      for (let col = 0; col < 16; col++) {
        const index = row * 16 + col, td = document.createElement("td"), button = document.createElement("button");
        if (col === 8) td.className = "page-start";
        button.type = "button"; button.className = "byte"; button.dataset.byte = index; button.tabIndex = -1;
        button.textContent = "—"; button.onclick = () => select(index);
        button.onkeydown = event => {
          const next = {ArrowLeft:index - 1, ArrowRight:index + 1, ArrowUp:index - 16, ArrowDown:index + 16,
            Home:event.ctrlKey ? 0 : row * 16, End:event.ctrlKey ? 255 : row * 16 + 15}[event.key];
          if (next !== undefined) {event.preventDefault(); select(Math.max(0, Math.min(255, next)), true);}
        };
        td.append(button); tr.append(td); cells[index] = button;
      }
      const ascii = document.createElement("td"); ascii.className = "ascii-cell"; ascii.setAttribute("aria-hidden", "true");
      for (let col = 0; col < 16; col++) {
        const index = row * 16 + col, span = document.createElement("span"); span.className = "ascii-char";
        span.onclick = () => select(index, true); ascii.append(span); asciiCells[index] = span;
      }
      tr.append(ascii); $("memory-body").append(tr);
    }
    for (let i = 7; i >= 0; i--) {
      const node = document.createElement("div"), label = document.createElement("small"), bit = document.createElement("span");
      node.className = "bit"; label.textContent = i; bit.textContent = "—"; node.append(label, bit); $("bits").append(node); bitCells[i] = bit;
    }
    const go = () => {
      const raw = $("address-input").value.trim();
      if (!/^(?:0x)?[0-9a-f]{1,2}$/i.test(raw)) {
        $("address-input").setCustomValidity("Укажите HEX-адрес от 00 до FF");
        $("address-input").reportValidity(); return;
      }
      select(parseInt(raw.replace(/^0x/i, ""), 16), true);
    };
    $("go").onclick = go;
    $("address-input").onkeydown = e => {if (e.key === "Enter") {e.preventDefault(); go();}};
    $("address-input").oninput = () => $("address-input").setCustomValidity("");
    $("pause").onclick = () => {
      paused = !paused; $("pause").setAttribute("aria-pressed", String(paused));
      $("pause").innerHTML = (paused ? playIcon() : pauseIcon()) + "<span>" + (paused ? "Продолжить" : "Пауза") + "</span>";
      if (!paused) shown = latest;
      render();
    };
    $("download").onclick = () => {
      if (!shown || shown.count !== SIZE || shown.stale || shown.presence !== "responding") return;
      const id = String(shown.id).replace(/[^a-zA-Z0-9_-]/g, "_").slice(0, 48) || "eeprom";
      const filename = id + "-" + stamp(shown.lastTime).replace(/[:.]/g, "-") + ".bin";
      const blob = new Blob([Uint8Array.from(shown.bytes.map(e => e.value))], {type:"application/octet-stream"});
      const url = URL.createObjectURL(blob), link = document.createElement("a");
      link.href = url; link.download = filename; root.append(link); link.click(); link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
      setText($("feedback"), "Снимок из 256 байт передан браузеру для сохранения.");
    };
  }
  window.bvstkView = {apiVersion:1, createTracker, mount,
    update(snapshot) {
      latest = tracker.accept(snapshot);
      if (!paused || !shown) {shown = latest; render();}
    }
  };
})();
