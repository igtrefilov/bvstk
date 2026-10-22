// PCF8563 indicators. Only physical cache data; the browser never advances RTC time.
(() => {
  "use strict";
  const TEMPLATE = `
<style>:root{--ink:#253540;--muted:#6d808d;--line:#d6e0e7;--canvas:#edf1f4;--paper:#fff;--nav:#303a43;--blue:#28648d;--blue-soft:#edf5fa;--green:#268352;--green-soft:#f1f8f4;--amber:#94661e;--amber-soft:#fff7e8;--red:#ac433c;--red-soft:#fff4f1;--sidebar:216px;--mono:ui-monospace,"Cascadia Code",Consolas,monospace}
*{box-sizing:border-box}body{margin:0;background:var(--canvas);color:var(--ink);font:13px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}button,select{font:inherit}button{cursor:pointer}button:focus-visible,a:focus-visible,summary:focus-visible,select:focus-visible{outline:2px solid #3c83b1;outline-offset:3px}button:disabled{cursor:default}[hidden]{display:none!important}svg{display:block}a{color:var(--blue)}
.icon{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round;flex:none}
.mono{font-family:var(--mono);font-variant-numeric:tabular-nums}
.muted{color:var(--muted)}
.square{width:7px;height:7px;display:inline-block;flex:none;border-radius:1px;background:var(--green)}
.square.offline{background:#98a6af}
.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0,0,0,0)}
.button{display:inline-flex;align-items:center;justify-content:center;gap:7px;min-height:31px;padding:5px 10px;background:#fff;border:1px solid #c4d2dc;border-radius:3px;color:#4b6578;font-size:11px}
.button:hover{background:#f5f9fc}
.button .icon{width:14px;height:14px}
.button[aria-pressed=true]{background:var(--amber-soft);border-color:#d9bd8b;color:var(--amber)}
.workspace-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:0 0 12px}
.workspace-head h2{font-size:16px;font-weight:600;margin:0}
.display-controls{display:flex;align-items:center;gap:11px}
.pause-note{font-size:10px;color:var(--amber);max-width:220px;text-align:right}
.panel{background:var(--paper);border:1px solid var(--line);border-radius:4px;min-width:0;overflow:hidden}
.panel-head{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:14px 18px;border-bottom:1px solid #e3eaf0;min-height:49px}
.panel-head h3{display:flex;align-items:center;gap:8px;margin:0;font-size:12px;font-weight:650}
.panel-head .icon{width:17px;height:17px;color:#7e97a9}
.panel-head small{color:var(--muted);font-size:10px}
.badge{display:inline-flex;align-items:center;gap:5px;min-height:23px;padding:3px 7px;border:1px solid transparent;border-radius:3px;background:#f0f3f6;color:#6f808e;white-space:nowrap;font-size:10px;font-weight:500}
.badge.ok{color:#2f7650;background:var(--green-soft);border-color:#d7e6dd}
.badge.blue{color:#356584;background:#eef5fa;border-color:#d9e7f0}
.badge.warn{color:#8c641e;background:var(--amber-soft);border-color:#ead8b4}
.badge.bad{color:#a6473e;background:var(--red-soft);border-color:#edd0c9}
.badge.neutral{color:var(--ink);border-color:#e0e5e9}
.alert{display:flex;align-items:flex-start;gap:10px;padding:12px 16px;border:1px solid #e5cfa6;background:var(--amber-soft);border-radius:4px;margin-bottom:14px;color:#8a6325}
.alert .icon{width:18px;height:18px;margin-top:2px}
.alert strong{display:block;font-size:12px;margin-bottom:3px;font-weight:650}
.alert p{margin:0;font-size:11px;line-height:1.6}
.alert.bad{background:var(--red-soft);border-color:#e9c7be;color:var(--red)}
.alert.neutral{background:#f5f8fa;border-color:#d4dee5;color:#647d90}
.overview-grid{display:grid;grid-template-columns:minmax(0,1.63fr) minmax(310px,1fr);gap:16px;align-items:stretch}
.clock-panel{border-top:3px solid #6b96b4;display:flex;flex-direction:column}
.clock-panel .panel-head{border-bottom:0;padding-top:12px}
.clock-panel.warning{border-top-color:#b68b42}
.clock-panel.invalid{border-top-color:#b86559}
.clock-main{padding:0 23px 13px;display:flex;align-items:center;gap:20px;justify-content:space-between;min-height:147px}
.clock-digits{display:flex;align-items:baseline;color:#2d526b;font-family:var(--mono);font-size:clamp(42px,4.5vw,68px);line-height:1.2;letter-spacing:-2.5px;font-weight:450;font-variant-numeric:tabular-nums;white-space:nowrap}
.clock-digits .colon{color:#93a9b8;font-size:.66em;align-self:center;position:relative;top:-2px;padding:0 4px}
.clock-digits.warning{color:#9a702f}
.clock-digits.invalid{color:#a4594f}
.time-caption{font-size:10px;color:var(--muted);margin:3px 0 11px}
.calendar-line{display:flex;align-items:baseline;flex-wrap:wrap;gap:10px;font-size:19px;font-weight:500;letter-spacing:-.3px}
.calendar-line small{font-size:12px;font-weight:400;color:var(--muted);letter-spacing:0}
.dial{width:112px;height:112px;flex:none;color:#a4b5c1;margin:0 5px 0 0}
.dial .face{fill:#f9fbfc;stroke:#d6e0e7;stroke-width:1}
.dial .marks{stroke:#b4c3cd;stroke-width:1.3}
.dial .hand{stroke:#69869b;stroke-width:2;stroke-linecap:round}
.dial .minute{stroke-width:1.5}
.dial .seconds{stroke:#91aec1;stroke-width:1}
.calendar-meta{display:flex;align-items:center;flex-wrap:wrap;gap:8px 22px;padding:12px 23px;background:#f9fbfc;border-top:1px solid #e4ebf0;font-size:11px;color:var(--muted);margin-top:auto}
.calendar-meta b{font-weight:500;color:#405d70;margin-left:6px}
.clock-foot{padding:9px 23px 12px;font-size:10px;color:#7d8e9a}
.clock-foot .mono{color:#48667c}
.partial-fields{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;padding:2px 23px 16px}
.partial-cell{background:#f7f9fb;border:1px solid #e1e8ed;padding:8px 11px;border-radius:3px;min-width:0}
.partial-cell span{display:block;font-size:10px;color:var(--muted)}
.partial-cell strong{display:block;color:#3e627d;font:25px/1.4 var(--mono)}
.partial-cell small{display:block;color:#8397a5;font-size:9px;font-family:var(--mono)}
.partial-cell.missing strong{color:#a4b1ba}
.partial-note{padding:0 23px 13px;margin:0;color:#8e722f;font-size:10px}
.validity-body{padding:17px 18px}
.validity-result{display:flex;gap:12px;align-items:center;margin-bottom:16px}
.validity-result .status-icon{width:33px;height:33px;color:var(--green);padding:6px;background:#eff7f2;border-radius:4px}
.validity-result.warn .status-icon{color:var(--amber);background:var(--amber-soft)}
.validity-result.bad .status-icon{color:var(--red);background:var(--red-soft)}
.validity-result.neutral .status-icon{color:#7e909e;background:#f0f4f7}
.validity-result strong{display:block;font-size:13px;font-weight:600}
.validity-result small{display:block;margin-top:3px;font-size:10px;color:var(--muted)}
.kv{margin:0;display:grid;grid-template-columns:minmax(0,1fr) auto;gap:9px 12px;font-size:11px;align-items:start}
.kv dt{color:var(--muted)}
.kv dd{margin:0;text-align:right;color:#3e596d;min-width:0;overflow-wrap:anywhere}
.kv code{font-size:10px;color:#7a91a0;margin-left:5px}
.quality-footer{display:flex;align-items:center;gap:10px;padding:11px 18px;border-top:1px solid #e3eaf0;font-size:10px;color:var(--muted);margin-top:auto}
.validity-panel{display:flex;flex-direction:column}
.coverage-track{display:flex;gap:2px;margin-left:auto}
.coverage-track i{width:5px;height:10px;background:#e2e9ee}
.coverage-track i.known{background:#90adbf}
.quality-footer strong{color:#4b677b;font-weight:500;white-space:nowrap}
.function-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px;margin-top:16px}
.function-panel{display:flex;flex-direction:column}
.function-body{padding:15px 18px 14px;flex:1}
.function-value{font-family:var(--mono);color:#345a75;font-size:31px;line-height:1.3;letter-spacing:-.8px;margin:0}
.function-value small{font:11px system-ui,sans-serif;color:var(--muted);letter-spacing:0;margin-left:4px}
.function-caption{margin:4px 0 15px;font-size:10px;color:var(--muted)}
.alarm-parts{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:5px;margin-bottom:16px}
.alarm-part{padding:6px 4px;text-align:center;background:#f0f5f9;border:1px solid #dce7ef;border-radius:3px}
.alarm-part span{display:block;font-size:9px;color:#7a8e9d;margin-bottom:3px}
.alarm-part strong{font-size:11px;font-weight:550;color:#446a84}
.alarm-part.ignored{border-style:dashed;border-color:#dde3e8;background:#fafbfc}
.alarm-part.ignored strong{color:#8294a1;font-weight:400}
.mini-line{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-top:8px;font-size:11px}
.mini-line .label{color:var(--muted)}
.mini-line .flag{font-size:10px;color:#7c91a0;margin-left:6px}
.function-footer{padding:10px 18px;border-top:1px solid #e6edf2;background:#fcfdfe;color:#7d909c;font-size:10px;min-height:39px}
.source-button{border:0;background:transparent;color:#5d7b91;padding:0;font:10px var(--mono);text-decoration:underline;text-decoration-color:#c9d9e4;text-underline-offset:3px}
.source-button:hover{color:var(--blue)}
.waveform{color:#7597af;width:100%;height:42px;margin:3px 0 12px}
.waveform path{stroke:currentColor;stroke-width:1.4;fill:none}
.waveform .baseline{stroke:#d9e5ed;stroke-width:1;stroke-dasharray:3 4}
.waveform.off{color:#b4c1ca}
.frequency-options{display:flex;gap:5px;flex-wrap:wrap;margin-bottom:13px}
.frequency-options span{font:9px var(--mono);color:#93a2ad;border:1px solid #e1e7ec;border-radius:3px;padding:3px 5px}
.frequency-options .selected{color:#396783;border-color:#bbd0df;background:#eff6fa}
.timer-metrics{margin:13px 0 16px}
.timer-hint{margin:11px 0 0;font-size:10px;color:#80929e;min-height:30px}
.irq-panel{margin-top:16px;display:grid;grid-template-columns:190px minmax(0,1fr);gap:8px;align-items:center;padding:17px 18px}
.irq-heading h3{font-size:12px;margin:0 0 4px;display:flex;align-items:center;gap:8px}
.irq-heading .icon{width:17px;height:17px;color:#7e97a9}
.irq-heading p{font-size:10px;margin:0;color:var(--muted)}
.irq-flow{display:grid;grid-template-columns:1fr 24px 1fr 24px 1.1fr;gap:9px;align-items:center}
.irq-node{padding:8px 11px;background:#f7f9fb;border:1px solid #e1e8ed;border-radius:3px;min-width:0}
.irq-node strong{display:block;color:#59778d;font-size:11px;font-weight:600}
.irq-node small{color:#879ba9;font:9px var(--mono)}
.irq-operator{color:#8b9eac;font-size:17px;text-align:center}
.irq-result{background:#f4f8f5;border-color:#dce7e0}
.irq-result strong{color:#3e7857}
.irq-node.active{background:var(--amber-soft);border-color:#e5d2ae}
.irq-node.active strong{color:var(--amber)}
.irq-node.unknown{background:#f3f6f8;border-color:#dce4e9}
.irq-node.unknown strong{color:#778b9a}
.irq-note{grid-column:1/-1;margin:4px 0 0;font-size:10px;color:#82939f}
.details-panel{margin-top:14px}
.details-panel summary{display:flex;align-items:center;gap:9px;padding:13px 17px;font-size:11px;color:#506d80;cursor:pointer;list-style:none}
.details-panel summary::-webkit-details-marker{display:none}
.details-panel summary .icon{height:15px;width:15px;color:#839aaa}
.details-panel summary .chevron{margin-left:auto;transform:rotate(-90deg);width:12px;height:12px}
.details-panel[open]>summary .chevron{transform:none}
.summary-meta{font-size:10px;color:#8a9ba7;margin-left:3px}
.register-layout{padding:3px 17px 17px;display:grid;grid-template-columns:minmax(0,1fr) 300px;gap:17px}
.register-groups{display:grid;gap:11px}
.register-group{display:flex;gap:12px;align-items:center}
.group-label{width:103px;flex:none;font-size:10px;color:#7d93a2}
.bytes{display:flex;gap:5px;flex-wrap:wrap}
.byte{min-width:44px;padding:6px 8px;background:#f7f9fb;border:1px solid #d9e3eb;border-radius:3px;color:#476983;font-family:var(--mono)}
.byte small{display:block;font-size:9px;color:#849bab;margin-bottom:3px}
.byte b{font-size:13px;font-weight:500}
.byte[aria-pressed=true]{border-color:#6597bb;background:#eaf3fa;box-shadow:0 0 0 1px #8aacc51c}
.byte.missing{border-style:dashed;color:#a1afb9}
.register-inspector{background:#f8fafc;border:1px solid #dce5eb;border-radius:3px;padding:12px 14px}
.register-inspector h4{margin:0 0 8px;font-size:11px;font-weight:600;overflow-wrap:anywhere}
.register-inspector p{margin:0;font-size:11px;color:#667f91;min-height:35px}
.inspector-value{font:25px var(--mono);color:#355c78;display:block;margin:8px 0}
.inspector-time{font-size:10px;color:#8094a2;display:block;margin-top:10px}
.source-note{margin:0;padding:0 17px 14px;font-size:10px;color:#8a9aa6}
.interpretation{padding:0 17px 17px;display:grid;grid-template-columns:1fr 1fr;gap:10px 24px;font-size:11px;color:#6f8492}
.interpretation p{margin:0}
.interpretation b{font-weight:600;color:#4c6a7d;display:block;margin-bottom:3px}
.bottom-note{display:flex;gap:12px;justify-content:space-between;flex-wrap:wrap;margin-top:16px;color:#8294a1;font-size:10px}
.bottom-note a{color:#64859d;text-underline-offset:3px}

@media(max-width:1100px){.clock-digits{font-size:54px}
.dial{width:88px;height:88px}
.alarm-parts{grid-template-columns:1fr 1fr}
.irq-panel{grid-template-columns:1fr}
.register-layout{grid-template-columns:1fr}}
@media(max-width:800px){.overview-grid{grid-template-columns:1fr}
.validity-body{display:grid;grid-template-columns:1fr 1fr;gap:18px}
.validity-result{margin:0}
.function-grid{grid-template-columns:1fr 1fr}
.function-panel:last-child{grid-column:1/-1}
.clock-digits{font-size:64px}
.dial{width:112px;height:112px}
.register-group{flex-direction:column;align-items:flex-start;gap:6px}
.group-label{width:auto}}
@media(max-width:500px){.clock-main{padding:0 14px 16px;gap:8px;min-height:135px}
.clock-digits{font-size:42px;letter-spacing:-1.7px}
.clock-digits .colon{padding:0 1px}
.dial{width:70px;height:70px}
.calendar-line{font-size:17px}
.calendar-meta{padding:10px 14px;gap:6px 12px}
.clock-foot{padding:8px 14px 12px}
.validity-body{grid-template-columns:1fr;padding:14px}
.function-grid{grid-template-columns:1fr}
.function-panel:last-child{grid-column:auto}
.alarm-parts{grid-template-columns:repeat(4,minmax(0,1fr))}
.irq-flow{grid-template-columns:1fr;gap:6px}
.irq-operator.arrow{transform:rotate(90deg)}
.partial-fields{padding:0 14px 13px;gap:7px}
.partial-cell{padding:6px}
.partial-cell strong{font-size:23px}
.partial-cell small{font-size:8px}
.summary-meta{display:none}
.pause-note{max-width:100px;font-size:9px}
.interpretation{grid-template-columns:1fr}}
body{background:#fff}#device-root{padding:3px}
.clock-digits{font-size:64px}
.function-footer{display:flex;align-items:center;flex-wrap:wrap;gap:4px}
.function-footer time{margin-left:auto;font-family:var(--mono);font-size:9px}
.aged{opacity:.72}
.bad-value{color:var(--red)!important}
.validity-result.neutral .status-icon{color:var(--muted);background:#f1f5f8}
.alarm-parts .invalid strong{color:var(--red)}
@media(max-width:1100px){.clock-digits{font-size:54px}}@media(max-width:500px){.clock-digits{font-size:40px}
.workspace-head h2{font-size:15px}
.panel-head,.function-body{padding-left:14px;padding-right:14px}
.irq-node{min-width:0}
.function-footer{font-size:9px}}
</style>
<svg width="0" height="0" style="position:absolute;overflow:hidden" aria-hidden="true">
<defs>
<symbol id="i-clock" viewBox="0 0 24 24">
<circle cx="12" cy="12" r="9"/>
<path d="M12 6v6l4 2"/>
</symbol>
<symbol id="i-shield" viewBox="0 0 24 24">
<path d="M12 3 4 6v6c0 5 8 9 8 9s8-4 8-9V6zM8 12l3 3 5-6"/>
</symbol>
<symbol id="i-bell" viewBox="0 0 24 24">
<path d="M18 8a6 6 0 0 0-12 0v6l-2 3h16l-2-3zM10 21h4M12 2V1"/>
</symbol>
<symbol id="i-timer" viewBox="0 0 24 24">
<circle cx="12" cy="14" r="8"/>
<path d="M9 2h6M12 2v4m6 1 2-2M12 9v6l-3 2"/>
</symbol>
<symbol id="i-wave" viewBox="0 0 24 24">
<path d="M2 17h4V7h7v10h7V7h2"/>
</symbol>
<symbol id="i-info" viewBox="0 0 24 24">
<circle cx="12" cy="12" r="9"/>
<path d="M12 11v6M12 7v1"/>
</symbol>
<symbol id="i-alert" viewBox="0 0 24 24">
<path d="m12 3 10 18H2zM12 9v5m0 3v1"/>
</symbol>
<symbol id="i-table" viewBox="0 0 24 24">
<rect x="3" y="4" width="18" height="16" rx="1"/>
<path d="M3 9h18M3 14h18M9 9v11m6-11v11"/>
</symbol>
<symbol id="i-eye" viewBox="0 0 24 24">
<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"/>
<circle cx="12" cy="12" r="3"/>
</symbol>
<symbol id="i-pause" viewBox="0 0 24 24">
<path d="M8 5v14M16 5v14" stroke-width="2.5"/>
</symbol>
<symbol id="i-play" viewBox="0 0 24 24">
<path d="m8 5 11 7-11 7z"/>
</symbol>
<symbol id="i-next" viewBox="0 0 24 24">
<path d="m5 5 10 7-10 7zM19 5v14"/>
</symbol>
</defs>
</svg>
<div class="workspace-head">
<h2>Показатели устройства</h2>
<div class="display-controls">
<span class="pause-note" id="pause-note" hidden>Отображение приостановлено</span>
<button class="button" id="pause" aria-pressed="false">
<svg class="icon">
<use href="#i-pause"/>
</svg>
<span>Пауза</span>
</button>
</div>
</div>
<div class="alert" id="alert" hidden role="status">
<svg class="icon">
<use href="#i-alert"/>
</svg>
<div>
<strong id="alert-title">
</strong>
<p id="alert-text">
</p>
</div>
</div>
<div class="overview-grid">
<section class="panel clock-panel" id="clock-panel" aria-labelledby="clock-title">
<div class="panel-head">
<h3 id="clock-title">
<svg class="icon">
<use href="#i-clock"/>
</svg>Время и календарь</h3>
<span class="badge blue" id="sample-quality">Одно чтение</span>
</div>
<div class="clock-main" id="combined-clock">
<div>
<div class="clock-digits" id="clock-digits" aria-label="Время устройства">
<span id="hours">14</span>
<span class="colon">:</span>
<span id="minutes">38</span>
<span class="colon">:</span>
<span id="seconds">52</span>
</div>
<p class="time-caption" id="time-caption">Время устройства в последнем снимке</p>
<div class="calendar-line">
<span id="calendar-date">22 октября</span>
<small>год <span class="mono" id="calendar-year">26</span>
</small>
</div>
</div>
<svg class="dial" id="dial" viewBox="0 0 120 120" aria-hidden="true">
<circle class="face" cx="60" cy="60" r="53"/>
<g class="marks">
<path d="M60 14v6M60 100v6M14 60h6M100 60h6"/>
<path d="m83 20-2 4m19 13-4 2m4 44-4-2m-13 19-2-4m-44 4 2-4m-19-13 4-2m-4-44 4 2m13-19 2 4"/>
</g>
<line id="hour-hand" class="hand" x1="60" y1="60" x2="60" y2="33"/>
<line id="minute-hand" class="hand minute" x1="60" y1="60" x2="60" y2="21"/>
<line id="second-hand" class="hand seconds" x1="60" y1="68" x2="60" y2="18"/>
<circle cx="60" cy="60" r="2.5" fill="#69869b"/>
</svg>
</div>
<div class="partial-fields" id="partial-fields" hidden>
</div>
<p class="partial-note" id="partial-note" hidden>
</p>
<div class="calendar-meta">
<span>Код дня недели<b class="mono" id="weekday">4</b>
</span>
<span>Бит века<b class="mono" id="century">C = 0</b>
</span>
<span>Век<b>Не задан</b>
</span>
</div>
<div class="clock-foot">Метка времени от старта <span class="mono" id="calendar-stamp">02:14:36.240</span>
</div>
</section>
<section class="panel validity-panel" aria-labelledby="validity-title">
<div class="panel-head">
<h3 id="validity-title">
<svg class="icon">
<use href="#i-shield"/>
</svg>Состояние часов</h3>
<button class="source-button" data-register="2">VL</button>
</div>
<div class="validity-body">
<div class="validity-result" id="validity-result">
<svg class="icon status-icon">
<use id="validity-symbol" href="#i-shield"/>
</svg>
<div>
<strong id="validity-value">Питание без отмеченного сбоя</strong>
<small id="validity-hint">Флаг потери достоверности сброшен</small>
</div>
</div>
<dl class="kv">
<dt>Счёт времени</dt>
<dd id="clock-state">Разрешён <code>STOP = 0</code>
</dd>
<dt>Режим</dt>
<dd id="test-state">Штатный</dd>
<dt>Календарь</dt>
<dd id="calendar-integrity">Согласованный снимок</dd>
</dl>
</div>
<div class="quality-footer">
<span>Получено регистров</span>
<div class="coverage-track" id="coverage" aria-hidden="true">
</div>
<strong id="coverage-label">16 из 16</strong>
</div>
</section>
</div>
<div class="function-grid">
<section class="panel function-panel" id="alarm-panel" aria-labelledby="alarm-title">
<div class="panel-head">
<h3 id="alarm-title">
<svg class="icon">
<use href="#i-bell"/>
</svg>Будильник</h3>
<span class="badge blue" id="alarm-state">Настроен</span>
</div>
<div class="function-body">
<p class="function-value" id="alarm-time">07:30</p>
<p class="function-caption" id="alarm-description">Ежедневное совпадение</p>
<div class="alarm-parts" id="alarm-parts">
</div>
<div class="mini-line">
<span class="label">Событие <span class="flag mono">AF</span>
</span>
<span id="alarm-event">Не зафиксировано</span>
</div>
<div class="mini-line">
<span class="label">Прерывание <span class="flag mono">AIE</span>
</span>
<span id="alarm-irq">Разрешено</span>
</div>
</div>
<div class="function-footer">
<button class="source-button" data-register="9">0x09–0x0C</button>
<span> Условия совпадения</span>
<time id="alarm-stamp">
</time>
</div>
</section>
<section class="panel function-panel" id="timer-panel" aria-labelledby="timer-title">
<div class="panel-head">
<h3 id="timer-title">
<svg class="icon">
<use href="#i-timer"/>
</svg>Таймер</h3>
<span class="badge blue" id="timer-state">Разрешён</span>
</div>
<div class="function-body">
<p class="function-value">
<span id="timer-count">45</span>
<small id="timer-unit">тактов в снимке</small>
</p>
<p class="function-caption">Текущее значение счётчика</p>
<dl class="kv timer-metrics">
<dt>Частота счёта</dt>
<dd id="timer-frequency">1 Гц</dd>
<dt>Оценка остатка</dt>
<dd id="timer-remaining">Около 45 с</dd>
</dl>
<div class="mini-line">
<span class="label">Событие <span class="flag mono">TF</span>
</span>
<span id="timer-event">Не зафиксировано</span>
</div>
<div class="mini-line">
<span class="label">Прерывание <span class="flag mono">TIE</span>
</span>
<span id="timer-irq">Разрешено</span>
</div>
</div>
<div class="function-footer">
<button class="source-button" data-register="15">0x0E–0x0F</button>
<span> Счётчик</span>
<time id="timer-stamp">
</time>
</div>
</section>
<section class="panel function-panel" id="clkout-panel" aria-labelledby="clkout-title">
<div class="panel-head">
<h3 id="clkout-title">
<svg class="icon">
<use href="#i-wave"/>
</svg>Выход CLKOUT</h3>
<span class="badge blue" id="clkout-state">Разрешён</span>
</div>
<div class="function-body">
<p class="function-value">
<span id="clkout-frequency">32 768</span>
<small>Гц</small>
</p>
<p class="function-caption">Установленная частота</p>
<svg class="waveform" id="waveform" viewBox="0 0 280 42" preserveAspectRatio="none" aria-label="Условная схема тактового сигнала">
<path class="baseline" d="M0 28H280"/>
<path id="wave-path" d="M0 29h16V10h31v19h31V10h31v19h31V10h31v19h31V10h31v19h31V10h16"/>
</svg>
<div class="frequency-options" id="frequency-options">
<span>32 768</span>
<span>1 024</span>
<span>32</span>
<span>1</span>
</div>
<div class="mini-line">
<span class="label">Режим вывода</span>
<span id="clkout-mode">Открытый сток</span>
</div>
</div>
<div class="function-footer">
<button class="source-button" data-register="13">0x0D</button>
<span> Настройка выхода</span>
<time id="clkout-stamp">
</time>
</div>
</section>
</div>
<section class="panel irq-panel" aria-labelledby="irq-title">
<div class="irq-heading">
<h3 id="irq-title">
<svg class="icon">
<use href="#i-wave"/>
</svg>Запрос прерывания</h3>
<p>Оценка по флагам и настройкам</p>
</div>
<div class="irq-flow">
<div class="irq-node" id="alarm-node">
<strong>Будильник</strong>
<small id="alarm-logic">AF = 0 &nbsp; AIE = 1</small>
</div>
<span class="irq-operator">+</span>
<div class="irq-node" id="timer-node">
<strong>Таймер</strong>
<small id="timer-logic">TF = 0 &nbsp; TIE = 1</small>
</div>
<span class="irq-operator arrow">→</span>
<div class="irq-node irq-result" id="irq-result">
<strong id="irq-value">Запроса нет</strong>
<small id="irq-mode">INT</small>
</div>
</div>
<p class="irq-note" id="irq-note">Электрический уровень на выводе INT здесь не измеряется.</p>
</section>
<details class="panel details-panel" id="raw-data">
<summary>
<svg class="icon">
<use href="#i-table"/>
</svg>Основания показателей<span class="summary-meta">Значения регистров и метки времени</span>
<svg class="icon chevron">
<use href="#i-chevron"/>
</svg>
</summary>
<div class="register-layout">
<div class="register-groups" id="register-groups">
</div>
<aside class="register-inspector">
<h4 id="inspector-name">
</h4>
<strong class="inspector-value" id="inspector-value">
</strong>
<p id="inspector-description">
</p>
<small class="inspector-time" id="inspector-time">
</small>
</aside>
</div>
<p class="source-note">Выберите байт для расшифровки. У каждого значения сохраняется собственная метка времени.</p>
</details>
<details class="panel details-panel" id="interpretation">
<summary>
<svg class="icon">
<use href="#i-info"/>
</svg>Как читать эти показатели<svg class="icon chevron">
<use href="#i-chevron"/>
</svg>
</summary>
<div class="interpretation">
<p>
<b>Полная дата</b>Год хранится двумя цифрами. Значение века, нумерация дней недели и часовой пояс задаются внешним ПО. Показаны двузначный год, бит века и числовой код дня недели.</p>
<p>
<b>Согласованность</b>Цельное время выводится при подтверждённом совместном чтении календаря. При раздельных чтениях доступны отдельные поля с их метками времени.</p>
<p>
<b>Контроль питания</b>VL сообщает о риске потери времени. По этому флагу нельзя определить напряжение или заряд батареи.</p>
<p>
<b>События и сигналы</b>Флаги AF и TF могут сохраняться после события. Частота CLKOUT показывает настройку. Оценка таймера относится к снимку; для точного отсчёта нужны два совпавших чтения счётчика.</p>
</div>
</details>

`;
  const AGE_MS = 5000;
  const pad = n => String(n).padStart(2, "0");
  const hex = n => "0x" + n.toString(16).toUpperCase().padStart(2, "0");
  const stamp = ms => Number.isSafeInteger(ms) && ms >= 0 ?
    [Math.floor(ms / 3600000), Math.floor(ms / 60000) % 60, Math.floor(ms / 1000) % 60].map(pad).join(":") + "." + String(ms % 1000).padStart(3, "0") : "—";
  const bcd = (raw, mask, min, max) => {
    if (!Number.isInteger(raw)) return null;
    const n = raw & mask, tens = n >> 4, ones = n & 15, value = tens * 10 + ones;
    return tens <= 9 && ones <= 9 && value >= min && value <= max ? value : null;
  };
  const months = ["января","февраля","марта","апреля","мая","июня","июля","августа","сентября","октября","ноября","декабря"];
  const names = ["Control_status_1","Control_status_2","VL_seconds","Minutes","Hours","Days","Weekdays","Century_months","Years","Minute_alarm","Hour_alarm","Day_alarm","Weekday_alarm","CLKOUT_control","Timer_control","Timer"];
  const explanations = [
    "STOP управляет счётом времени. TEST1 выбирает внешний такт. TESTC разрешает POR override.",
    "AF и TF фиксируют события. AIE и TIE разрешают INT. TI_TP выбирает импульсный режим таймера.",
    "Бит 7 — VL. Секунды хранятся в коде BCD в младших семи битах.",
    "Минуты в коде BCD, диапазон 00–59.", "Часы в коде BCD, диапазон 00–23.",
    "День месяца в коде BCD, диапазон 01–31.", "Код дня недели 0–6. Названия дней определяются соглашением ПО.",
    "Бит 7 — C. Месяц в коде BCD в младших пяти битах.", "Последние две цифры года в коде BCD.",
    "Бит 7 исключает минуты из сравнения.", "Бит 7 исключает часы из сравнения.",
    "Бит 7 исключает день месяца из сравнения.", "Бит 7 исключает день недели из сравнения.",
    "FE разрешает CLKOUT, FD выбирает частоту. В режиме TEST1 этот вывод принимает внешний такт.",
    "TE разрешает таймер. TD выбирает источник тактов.", "Двоичный счётчик 0–255. Таймер продолжает считать во время чтения."
  ];
  const fields = [["Часы",4,63,0,23],["Минуты",3,127,0,59],["Секунды",2,127,0,59],["День",5,63,1,31],["Месяц",7,31,1,12],["Год",8,255,0,99]];
  const calendarRegs = [2,3,4,5,6,7,8];
  const physical = entry => entry?.valid === true && ["I2C","read"].includes(entry.source) &&
    Number.isInteger(entry.val) && entry.val >= 0 && entry.val <= 255 &&
    Number.isSafeInteger(entry.updated_ms) && entry.updated_ms >= 0;
  // read_id is supplied only for a successful block read. Millisecond equality
  // alone cannot establish coherence: independent reads can finish in one tick.
  function decode(snapshot = {}) {
    const entries = Array(16).fill(null), seen = new Set();
    for (const entry of snapshot.cache || []) {
      const r = entry?.reg;
      if (!Number.isInteger(r) || r < 0 || r > 15) continue;
      entries[r] = seen.has(r) ? null : physical(entry) ? {...entry} : null;
      seen.add(r);
    }
    const values = entries.map(e => e?.val ?? null);
    const times = entries.map(e => e?.updated_ms ?? null);
    const bits = (r, mask) => entries[r] ? Boolean(values[r] & mask) : null;
    const parts = fields.map(([,r,mask,min,max]) => bcd(values[r],mask,min,max));
    const hasCalendar = calendarRegs.every(r => entries[r]);
    const invalidFields = fields.filter(([,r,mask,min,max]) => entries[r] && bcd(values[r],mask,min,max) === null).map(([,r])=>r);
    if (entries[6] && (values[6] & 7) > 6) invalidFields.push(6);
    // PCF8563 uses its two-digit year for leap-year correction, including 00.
    if (parts[3] !== null && parts[4] !== null && parts[5] !== null &&
        parts[3] > [31,parts[5]%4===0?29:28,31,30,31,30,31,31,30,31,30,31][parts[4]-1]) invalidFields.push(5);
    const first = entries[2];
    const coherent = hasCalendar && Number.isInteger(first.read_id) && first.read_id > 0 &&
      first.read_id <= 0xffffffff && Number.isInteger(first.read_duration_ms) &&
      first.read_duration_ms >= 0 && first.read_duration_ms < 1000 &&
      calendarRegs.every(r => entries[r].read_id === first.read_id &&
        entries[r].updated_ms === first.updated_ms && entries[r].read_duration_ms === first.read_duration_ms);
    const now = Number.isFinite(snapshot.uptime_ms) ? snapshot.uptime_ms : null;
    const disconnected = snapshot.stale === true || snapshot.device?.presence !== "responding";
    const fresh = entries.map(e => !!e && !disconnected && now !== null && Math.max(0, now-e.updated_ms) <= AGE_MS);
    const stale = disconnected || entries.some((e,r) => e && !fresh[r]);
    const alarmRegs = [10,9,11,12], excluded = alarmRegs.map(r=>bits(r,128));
    const alarmValues = [bcd(values[10],63,0,23),bcd(values[9],127,0,59),bcd(values[11],63,1,31),
      entries[12] && (values[12]&7)<7 ? values[12]&7 : null];
    const alarmKnown = alarmRegs.every(r=>entries[r]);
    const alarmInvalid = excluded.some((skip,i)=>skip===false && alarmValues[i]===null);
    const alarmEnabled = excluded.filter(skip=>skip===false).length;
    return {entries,values,times,fresh,stale,disconnected,parts,hasCalendar,coherent,invalidFields,
      combined:coherent && !invalidFields.length,count:entries.filter(Boolean).length,
      stop:bits(0,32),vl:bits(2,128),test1:bits(0,128),testc:bits(0,8),
      af:bits(1,8),tf:bits(1,4),aie:bits(1,2),tie:bits(1,1),pulse:bits(1,16),
      te:bits(14,128),td:entries[14]?values[14]&3:null,
      fe:bits(13,128),fd:entries[13]?values[13]&3:null,
      alarmRegs,excluded,alarmValues,alarmKnown,alarmInvalid,alarmEnabled};
  }

  let root, shown, pending, paused = false, selected = 2;
  const $ = id => root.querySelector("#"+id);
  const text = (id,value) => {const node=$(id), next=String(value);if(node.textContent!==next)node.textContent=next;};
  const badge = (id,value,tone="neutral") => {text(id,value);$(id).className="badge "+tone;};
  function inspector() {
    const s=shown, e=s.entries[selected];
    text("inspector-name",hex(selected)+"  "+names[selected]);
    text("inspector-value",e ? hex(e.val) : "Нет данных");
    text("inspector-description",explanations[selected]);
    text("inspector-time",e ? "Метка времени "+stamp(e.updated_ms)+(s.fresh[selected]?"":"  Данные устарели") : "Ожидание чтения");
    root.querySelectorAll("[data-byte]").forEach(node=>node.setAttribute("aria-pressed",String(Number(node.dataset.byte)===selected)));
  }
  function panelState(id,regs) {
    const s=shown, known=regs.map(r=>s.entries[r]).filter(Boolean), fresh=regs.every(r=>s.fresh[r]);
    $(id+"-panel").classList.toggle("aged",known.length>0 && !fresh);
    const latest=known.length ? Math.max(...known.map(e=>e.updated_ms)) : null;
    text(id+"-stamp",known.length ? (fresh?"":"Устарело  ")+stamp(latest) : "Нет чтения");
    $(id+"-stamp").title="Последнее чтение полей карточки, время от старта системы";
  }
  function render() {
    const s=shown,v=s.values;
    const {stop,vl,test1,testc,af,tf,aie,tie,pulse,te,td,fe,fd}=s;
    $("combined-clock").hidden=!s.combined;
    $("partial-fields").hidden=s.combined;
    $("partial-note").hidden=s.combined;
    if (s.combined) {
      text("hours",pad(s.parts[0]));text("minutes",pad(s.parts[1]));text("seconds",pad(s.parts[2]));
      text("calendar-date",s.parts[3]+" "+months[s.parts[4]-1]);text("calendar-year",pad(s.parts[5]));
      $("hour-hand").setAttribute("transform","rotate("+(s.parts[0]%12*30+s.parts[1]/2)+" 60 60)");
      $("minute-hand").setAttribute("transform","rotate("+(s.parts[1]*6)+" 60 60)");
      $("second-hand").setAttribute("transform","rotate("+(s.parts[2]*6)+" 60 60)");
    }
    fields.forEach(([,r],i)=>{
      const node=root.querySelector('[data-field="'+r+'"]'),invalid=s.invalidFields.includes(r);
      node.classList.toggle("missing",!s.entries[r]);
      node.classList.toggle("aged",!!s.entries[r]&&!s.fresh[r]);
      node.querySelector("strong").textContent=invalid?"Ошибка":s.parts[i]===null?"—":pad(s.parts[i]);
      node.querySelector("strong").classList.toggle("bad-value",invalid);
      node.querySelector("small").textContent=s.entries[r]?stamp(s.times[r]):"Нет чтения";
    });
    text("partial-note",s.invalidFields.length?"Проверьте поля с недопустимым кодом или датой.":
      s.hasCalendar?"Показаны отдельные поля. Совместное чтение календаря не подтверждено.":"Доступные поля заполняются по мере чтения.");
    text("weekday",s.entries[6]?(v[6]&7)<7?String(v[6]&7):"Ошибка":"—");
    text("century",s.entries[7]?"C = "+((v[7]>>7)&1):"—");
    text("calendar-stamp",s.combined?stamp(s.times[2]):"Указана у каждого поля");
    text("time-caption",s.stale?"Сохранённое время устройства":vl?"Время требует проверки":stop?"Остановленное время устройства":"Время устройства в последнем снимке");
    badge("sample-quality",s.invalidFields.length?"Ошибка значения":!s.hasCalendar?"Неполный кэш":
      s.stale?"Устарел":s.coherent?"Одно чтение":"Раздельные чтения",
      s.invalidFields.length?"bad":s.stale?"warn":s.coherent?"blue":"neutral");
    $("clock-panel").className="panel clock-panel"+(vl||s.invalidFields.length?" invalid":stop||s.stale?" warning":"");
    $("clock-digits").className="clock-digits"+(vl?" invalid":stop||s.stale?" warning":"");
    text("validity-value",vl===null?"Достоверность неизвестна":vl?"Время требует проверки":"Провал питания не отмечен");
    text("validity-hint",vl===null?"Регистр секунд ещё не прочитан":vl?"VL = 1. Возможна потеря календаря":"VL = 0. Флаг потери достоверности сброшен");
    $("validity-result").className="validity-result"+(vl===null||!s.fresh[2]?" neutral":vl?" bad":"");
    $("validity-symbol").setAttribute("href",vl?"#i-alert":"#i-shield");
    text("clock-state",stop===null?"Нет данных":stop?"Остановлен  STOP = 1":"Разрешён  STOP = 0");
    text("test-state",test1===null?"Нет данных":[test1?"EXT_CLK":"",testc?"POR override":""].filter(Boolean).join(", ")||"Штатный");
    text("calendar-integrity",!s.hasCalendar?"Неполные данные":s.invalidFields.length?"Ошибка значения":s.coherent?"Согласованный снимок":"Раздельные поля");
    text("coverage-label",s.count+" из 16");
    $("coverage").querySelectorAll("i").forEach((node,i)=>node.classList.toggle("known",!!s.entries[i]));

    const {excluded,alarmValues,alarmKnown,alarmEnabled,alarmInvalid}=s;
    $("alarm-parts").querySelectorAll(".alarm-part").forEach((node,i)=>{
      node.classList.toggle("ignored",excluded[i]!==false);
      node.classList.toggle("invalid",excluded[i]===false&&alarmValues[i]===null);
      node.querySelector("strong").textContent=excluded[i]===null?"—":excluded[i]?"Любой":
        alarmValues[i]===null?"Ошибка":i===3?String(alarmValues[i]):pad(alarmValues[i]);
    });
    text("alarm-time",!alarmKnown?"—":alarmInvalid?"Ошибка":!alarmEnabled?"Отключён":
      excluded[0]===false&&excluded[1]===false?pad(alarmValues[0])+":"+pad(alarmValues[1]):"По условиям");
    text("alarm-description",!alarmKnown?"Ожидание регистров будильника":alarmInvalid?"Некорректное условие сравнения":
      !alarmEnabled?"Все сравнения исключены":excluded[2]&&excluded[3]&&!excluded[0]&&!excluded[1]?
      "Ежедневное совпадение":"Совпадение включённых условий");
    badge("alarm-state",!alarmKnown?"Нет данных":alarmInvalid?"Ошибка":af?"Событие":alarmEnabled?"Настроен":"Выключен",
      alarmInvalid?"bad":af?"warn":alarmEnabled&&alarmKnown?"blue":"neutral");
    text("alarm-event",af===null?"Нет данных":af?"Зафиксировано":"Не зафиксировано");
    $("alarm-event").style.color=af?"var(--amber)":"";
    text("alarm-irq",aie===null?"Нет данных":aie?"Разрешено":"Запрещено");
    panelState("alarm",[1,9,10,11,12]);

    const timerUnknown=te===null||stop===null||test1===null;
    badge("timer-state",timerUnknown?"Нет данных":!te?"Выключен":stop?"Счёт остановлен":test1?"Внешний такт":"Разрешён",
      stop||test1?"warn":te?"blue":"neutral");
    text("timer-count",s.entries[15]?v[15]:"—");text("timer-unit",s.entries[15]?"тактов в снимке":"");
    text("timer-frequency",td===null?"Нет данных":["4 096 Гц","64 Гц","1 Гц","1 такт в минуту"][td]);
    // Count is a sample, not a promise of time remaining. Timer is not latched
    // by an I2C read. TEST1 replaces the internal 64 Hz clock.
    text("timer-remaining",te&&stop===false&&test1===false&&td!==null&&s.entries[15]&&v[15]>0?
      "Около "+(v[15]/[4096,64,1,1/60][td]).toLocaleString("ru-RU",{maximumFractionDigits:3})+" с":
      te&&stop?"Счёт остановлен":te&&test1?"Внешний такт":"—");
    text("timer-event",tf===null?"Нет данных":tf?"Зафиксировано":"Не зафиксировано");
    $("timer-event").style.color=tf?"var(--amber)":"";
    text("timer-irq",tie===null?"Нет данных":tie?"Разрешено":"Запрещено");
    panelState("timer",[0,1,14,15]);

    const outputStopped=fe&&stop&&fd!==0;
    const clkUnknown=fe===null||test1===null||stop===null;
    badge("clkout-state",clkUnknown?"Нет данных":test1?"Тестовый вход":!fe?"Выключен":outputStopped?"Остановлен":"Разрешён",
      test1||outputStopped?"warn":fe&&!clkUnknown?"blue":"neutral");
    text("clkout-frequency",fd===null?"—":["32 768","1 024","32","1"][fd]);
    text("clkout-mode",clkUnknown?"Нет данных":test1?"Вход EXT_CLK":!fe?"Высокоимпедансный":"Открытый сток");
    const wave=fe&&!clkUnknown&&!outputStopped&&!test1;
    $("waveform").classList.toggle("off",!wave);
    $("wave-path").setAttribute("d",wave?"M0 29h16V10h31v19h31V10h31v19h31V10h31v19h31V10h31v19h31V10h16":"M0 29H280");
    $("frequency-options").querySelectorAll("span").forEach((node,i)=>node.classList.toggle("selected",fd===i));
    panelState("clkout",[0,13]);

    const irqKnown=!!s.entries[1],alarmRequest=af&&aie,timerRequest=tf&&tie;
    text("alarm-logic",irqKnown?"AF = "+Number(af)+"   AIE = "+Number(aie):"Нет данных");
    text("timer-logic",irqKnown?"TF = "+Number(tf)+"   TIE = "+Number(tie):"Нет данных");
    $("alarm-node").classList.toggle("active",!!alarmRequest);
    $("timer-node").classList.toggle("active",!!timerRequest);
    $("irq-result").className="irq-node irq-result"+(!irqKnown?" unknown":alarmRequest||timerRequest?" active":"");
    $("irq-result").classList.toggle("aged",irqKnown&&!s.fresh[1]);
    text("irq-value",!irqKnown?"Нет данных":alarmRequest&&timerRequest?"Оба источника":
      alarmRequest?"Запрос будильника":timerRequest?pulse?"Импульсный режим":"Запрос таймера":"Запроса нет");
    text("irq-mode",!irqKnown?"INT":alarmRequest?"AF и AIE установлены":timerRequest&&pulse?
      "Текущий уровень неизвестен":"INT по регистру");
    text("irq-note",irqKnown&&tf&&!tie?
      "TF установлен, передача события таймера на INT запрещена. Уровень вывода не измеряется.":
      irqKnown&&pulse?"Импульс таймера может завершиться до чтения TF. Уровень вывода не измеряется.":
      "Электрический уровень на выводе INT здесь не измеряется.");

    let title="",message="",tone="";
    if(s.disconnected){title="Нет свежего ответа";message=s.count?"Показаны последние доступные значения с метками чтения.":"Ожидание ответа устройства.";tone="neutral";}
    else if(vl){title="Время требует проверки";message="Установлен VL. Календарь мог быть потерян при нарушении питания.";tone="bad";}
    else if(s.invalidFields.length){title="Некорректный календарь";message="Проверьте регистры "+s.invalidFields.map(hex).join(", ")+". Цельное время недоступно.";tone="bad";}
    else if(s.stale){title="Данные устарели";message="Часть значений не обновлялась более пяти секунд. Метки указывают время последнего чтения.";tone="neutral";}
    else if(stop){title="Счёт времени остановлен";message="Установлен STOP. Показания сохраняются до возобновления счёта.";}
    else if(!s.hasCalendar){title=s.count?"Получена часть данных":"Ожидание данных RTC";message="Известные поля показаны отдельно. Карточки заполняются по мере чтения регистров.";tone="neutral";}
    else if(!s.coherent){title="Календарь получен раздельно";message="Для цельного времени требуется подтверждённое совместное чтение длительностью менее секунды.";}
    $("alert").hidden=!title;$("alert").className="alert"+(tone?" "+tone:"");
    text("alert-title",title);text("alert-text",message);
    root.querySelectorAll("[data-byte]").forEach(node=>{
      const r=Number(node.dataset.byte);
      node.querySelector("b").textContent=s.entries[r]?hex(v[r]).slice(2):"—";
      node.classList.toggle("missing",!s.entries[r]);
      node.title=hex(r)+" "+names[r]+" — "+(s.entries[r]?stamp(s.times[r]):"Нет данных");
    });
    inspector();
  }
  window.bvstkView = {
    apiVersion:1,
    decode,
    mount(target) {
      root=target;root.innerHTML=TEMPLATE;paused=false;selected=2;
      for(const [label,indices] of [["Управление",[0,1]],["Календарь",[2,3,4,5,6,7,8]],["Будильник",[9,10,11,12]],["Выход и таймер",[13,14,15]]]) {
        const group=document.createElement("div");group.className="register-group";
        const title=document.createElement("span");title.className="group-label";title.textContent=label;group.append(title);
        const bytes=document.createElement("div");bytes.className="bytes";
        for(const r of indices) {
          const node=document.createElement("button");node.type="button";node.className="byte";
          node.dataset.byte=r;node.innerHTML="<small>"+hex(r)+"</small><b>—</b>";
          node.addEventListener("click",()=>{selected=r;inspector();});bytes.append(node);
        }
        group.append(bytes);$("register-groups").append(group);
      }
      for(const [label,r] of fields) {
        const cell=document.createElement("div");cell.className="partial-cell";cell.dataset.field=r;
        cell.innerHTML="<span>"+label+"</span><strong>—</strong><small>—</small>";$("partial-fields").append(cell);
      }
      for(const label of ["Часы","Минуты","День","День недели"]) {
        const cell=document.createElement("div");cell.className="alarm-part";
        cell.innerHTML="<span>"+label+"</span><strong>—</strong>";$("alarm-parts").append(cell);
      }
      for(let r=0;r<16;r++)$("coverage").append(document.createElement("i"));
      $("pause").addEventListener("click",()=>{
        paused=!paused;$("pause").setAttribute("aria-pressed",String(paused));
        $("pause").querySelector("span").textContent=paused?"Продолжить":"Пауза";
        $("pause").querySelector("use").setAttribute("href",paused?"#i-play":"#i-pause");
        $("pause-note").hidden=!paused;
        if(!paused){shown=pending;render();}
      });
      root.querySelectorAll("[data-register]").forEach(button=>button.addEventListener("click",()=>{
        selected=Number(button.dataset.register);$("raw-data").open=true;inspector();
        root.querySelector('[data-byte="'+selected+'"]').focus({preventScroll:true});
      }));
      shown=pending=decode();render();
    },
    update(snapshot) {
      pending=decode(snapshot);
      if(!paused){shown=pending;render();}
    }
  };
})();
