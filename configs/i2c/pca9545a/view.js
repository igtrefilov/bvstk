// PCA9545A device view API v1. Snapshot-only; no bus or network operations.
(() => {
  "use strict";
  const template = `<style>:root{--ink:#23313b;--muted:#6b7c88;--line:#d8e0e5;--paper:#fff;--blue:#27658f;--green:#21874f;--amber:#956316;--amber-bg:#fff5e3;--mono:ui-monospace,"Cascadia Code",Consolas,monospace}
*{box-sizing:border-box}body{color:var(--ink);font:13px/1.5 Arial,sans-serif}button{font:inherit;cursor:pointer}button:focus-visible,summary:focus-visible{outline:2px solid #327dad;outline-offset:-2px}[hidden]{display:none!important}svg{display:block}.icon{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round;flex:none}.mono{font-family:var(--mono);font-variant-numeric:tabular-nums}.square{width:8px;height:8px;border-radius:1px;background:var(--green);flex:none;display:inline-block}.square.offline{background:#98a6af}
    .workspace-head { display:flex; justify-content:space-between; align-items:center; gap:14px; margin:3px 0 13px }
    .section-heading { display:flex; align-items:center; gap:9px }
    .section-heading h2 { font-size:16px; font-weight:600; letter-spacing:-.15px; margin:0 }
    .section-heading .icon { color:#728b9b; width:18px; height:18px }
    .main-grid { display:block }
    .panel { min-width:0; background:var(--paper); border:1px solid var(--line); border-radius:4px; overflow:hidden }
    .panel-title { min-height:52px; display:flex; justify-content:space-between; align-items:center; gap:12px; padding:14px 18px; border-bottom:1px solid var(--line) }
    .panel-title h3 { font-size:13px; font-weight:600; margin:0 }
    .panel-title small { font-size:10px; color:var(--muted) }
    .channels-head { display:grid; grid-template-columns:38px minmax(0,1fr); gap:12px; padding:11px 18px 10px; background:#fafcfd; border-bottom:1px solid #e9eef1; font-size:10px; color:#738693 }
    .channels-head .channel-label { grid-column:1 / 3 }
    .channel-list { position:relative; padding:0 0 0 18px }
    .channel-list::before { content:""; position:absolute; left:17px; top:37px; bottom:37px; width:1px; background:#ccd7df }
    .channel { position:relative; display:grid; grid-template-columns:38px minmax(0,1fr); gap:12px; align-items:center; width:100%; min-height:100px; padding:16px 18px 16px 16px; text-align:left; color:inherit; background:white; border:0; border-bottom:1px solid #e8edf1; border-radius:0 }
    .channel:last-child { border-bottom:0 }
    .channel::before { content:""; position:absolute; left:-1px; top:50%; width:17px; height:1px; background:#ccd7df }
    .channel:hover { background:#f9fbfc }
    .channel-number { width:32px; height:32px; display:flex; align-items:center; justify-content:center; background:#f0f3f5; border:1px solid #cbd7df; border-radius:4px; font:14px var(--mono); color:#6c8190; position:relative }
    .channel-main { min-width:0 }
    .channel-title { display:block; font-size:12px; font-weight:600; color:#3b5263; margin-bottom:7px }
    .devices { display:flex; flex-wrap:wrap; align-items:center; gap:6px 10px }
    .endpoint { display:inline-flex; align-items:center; gap:5px; color:#4b626f; font-size:10px; white-space:nowrap }
    .endpoint .icon { width:15px; height:15px; color:#8094a1 }
    .endpoint .square { width:5px; height:5px }
    .endpoint .addr { font:9px var(--mono); color:#7b8e9c }
    .empty-channel { font-size:11px; color:#8999a5 }
    .legend { display:flex; align-items:center; flex-wrap:wrap; gap:16px; padding:12px 18px; border-top:1px solid var(--line); color:#6b8090; font-size:10px }
    .legend span { display:inline-flex; align-items:center; gap:6px }
    .legend .square { width:6px; height:6px }
    .footer-note { display:flex; justify-content:space-between; align-items:flex-start; gap:15px; margin-top:18px; font-size:10px; color:#8595a0 }
    .footer-note a { color:#698498; text-decoration:none }
    .footer-note a:hover { text-decoration:underline }
@media(max-width:600px){.section-heading h2{font-size:15px}.channels-head{grid-template-columns:1fr;padding:10px 16px}.channels-head .channel-label{grid-column:1}
      .channel { grid-template-columns:32px minmax(0,1fr); gap:8px 13px; padding:17px 12px 17px 16px; min-height:96px } .channel-number { align-self:center; width:29px; height:29px } .channel-main { grid-column:2 }
      .channel-title { margin-bottom:6px } .devices { gap:6px 8px } .endpoint { font-size:10px } .channel-list::before { top:55px; bottom:55px }
      .legend { gap:10px; padding:12px 14px; font-size:9px }
      .footer-note { flex-direction:column; gap:6px }
    }
</style><svg width="0" height="0" style="position:absolute;overflow:hidden" aria-hidden="true"><defs><symbol id="i-bus" viewBox="0 0 24 24"><path d="M3 8h18M3 16h18M6 5v3m6-3v3m6-3v3M6 16v3m6-3v3m6-3v3"/><circle cx="6" cy="8" r="1.2" fill="currentColor"/><circle cx="18" cy="16" r="1.2" fill="currentColor"/></symbol><symbol id="i-chip" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 3v3m6-3v3M9 18v3m6-3v3M3 9h3m-3 6h3m12-6h3m-3 6h3"/></symbol><symbol id="i-memory" viewBox="0 0 24 24"><rect x="5" y="6" width="14" height="12" rx="1.5"/><path d="M8 3v3m4-3v3m4-3v3M8 18v3m4-3v3m4-3v3M8 10h8m-8 4h8"/></symbol><symbol id="i-sfp" viewBox="0 0 24 24"><path d="M5 6h14v13H5zM8 6V3h8v3M8 19v3h8v-3M5 10h14"/><rect x="8" y="12" width="3" height="4" rx=".4"/><rect x="13" y="12" width="3" height="4" rx=".4"/></symbol><symbol id="i-gpio" viewBox="0 0 24 24"><rect x="7" y="5" width="10" height="14" rx="1.5"/><path d="M2 8h5m-5 4h5m-5 4h5m10-8h5m-5 4h5m-5 4h5M10 9h4m-4 3h4m-4 3h4"/></symbol><symbol id="i-info" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7v1"/></symbol></defs></svg>  <div class="workspace-head"><div class="section-heading"><svg class="icon"><use href="#i-bus"/></svg><h2>Каналы и устройства</h2></div></div>
  <div class="main-grid">
    <div>
      <section class="panel" aria-labelledby="channels-title">
        <div class="panel-title"><h3 id="channels-title">Топология свитча</h3><small>4 канала</small></div>
        <div class="channels-head" aria-hidden="true"><span class="channel-label">Канал и профили устройств</span></div>
        <div class="channel-list" id="channels" aria-label="Каналы свитча."></div>
        <div class="legend"><span><i class="square"></i>Готово</span><span><i class="square offline"></i>Не готово</span></div>
      </section>
  </div>
`;
  const hex = n => '0x' + n.toString(16).toUpperCase().padStart(2,'0');
  const esc = value => String(value ?? '').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const icon = name => `<svg class="icon" aria-hidden="true"><use href="#i-${name}"/></svg>`;
  let root, displayed;
  const rows = new Map();
  const $ = id => root.querySelector('#'+id);
  const devices = () => Array.isArray(displayed?.routing?.children) ?
    displayed.routing.children.filter(d=>Number.isInteger(d.channel)&&d.channel>=0&&d.channel<4&&typeof d.name==='string').slice(0,32) : [];
  const addresses = d => Array.isArray(d.addresses) ? [...new Set(d.addresses.filter(n=>Number.isInteger(n)&&n>=0&&n<128))] : [];
  const deviceName = d => String(d.label || d.model || d.name);
  const ready = d => !displayed?.stale && d.presence==='responding';
  const hasTopology = () => Array.isArray(displayed?.routing?.children);

  function accept(snapshot) {
    displayed=snapshot;
    render();
  }

  function render() {
    const children=devices();
    for(let channel=0;channel<4;channel++) {
      const row=rows.get(channel), branch=children.filter(d=>d.channel===channel);
      const key=JSON.stringify([hasTopology(),branch.map(d=>[d.name,d.label,d.model,d.class,addresses(d)])]);
      if(row.dataset.children!==key) {
        row.dataset.children=key;
        row.querySelector('.devices').innerHTML=branch.length ? branch.map(d=>{
          const glyph={memory:'memory',gpio:'gpio',phy:'sfp',bridge:'bus'}[d.class] || 'chip';
          return `<span class="endpoint">${icon(glyph)}<span>${esc(deviceName(d))}</span><span class="addr">${addresses(d).map(hex).join(' ')}</span><i class="square" aria-hidden="true"></i></span>`;
        }).join('') : `<span class="empty-channel">${hasTopology() ? 'Профили не заданы' : 'Топология недоступна'}</span>`;
      }
      row.querySelectorAll('.endpoint').forEach((node,i)=>{
        const state=ready(branch[i])?'Готово':'Не готово';
        node.title=deviceName(branch[i])+' — '+state;
        node.querySelector('.square').classList.toggle('offline',!ready(branch[i]));
      });
      row.setAttribute('aria-label',`Канал ${channel}. `+
        (hasTopology()?branch.map(d=>deviceName(d)+': '+(ready(d)?'Готово':'Не готово')).join(', '):'Топология недоступна')+'.');
    }
  }

  window.bvstkView = {
    apiVersion:1,
    mount(target) {
      root=target; root.innerHTML=template;
      displayed=null; rows.clear();
      for(let channel=0;channel<4;channel++) {
        const row=document.createElement('div');row.className='channel';row.dataset.channel=channel;
        row.innerHTML=`<span class="channel-number">${channel}</span><span class="channel-main"><span class="channel-title">Канал ${channel}</span><span class="devices"></span></span>`;
        $('channels').append(row);rows.set(channel,row);
      }
      accept({device:{presence:'unknown'},routing:null,stale:false});
    },
    update(snapshot) {
      accept(snapshot);
    }
  };
})();
