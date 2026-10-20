// Values are decoded by register_map.json; no local running clock is simulated.
(() => {
  let root;
  const sections=[
    ["Календарь",["hours","minutes","seconds","day","month","year","century"]],
    ["Состояние",["clock","voltage","alarm","timer"]]
  ];
  window.bvstkView={
    apiVersion:1,
    mount(target) {
      root=target;
      root.innerHTML=`<style>
        .groups{display:grid;grid-template-columns:2fr 1fr;gap:18px}.group{padding:16px;border:1px solid #d7dfe4}
        h3{margin:0 0 12px;font-size:14px}.metrics{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:16px}
        span,small{display:block;color:#68737c;font-size:12px}strong{display:block;margin:6px 0;font-size:18px}
        p{color:#68737c;line-height:1.5;font-size:12px}.bad{color:#ac342b}@media(max-width:650px){.groups{grid-template-columns:1fr}}
      </style><div class="groups">${sections.map(([title,ids])=>`<section class="group"><h3>${title}</h3><div class="metrics">${ids.map(id=>`<div data-metric="${id}"><span></span><strong>Нет данных</strong><small></small></div>`).join("")}</div></section>`).join("")}</div>
      <p>Отдельные значения из кэша могут относиться к разным моментам. Согласованная дата и абсолютный век здесь не вычисляются. Для каждого поля показана метка последнего взаимодействия.</p>`;
    },
    update(snapshot) {
      for(const item of snapshot.summary) {
        const node=root.querySelector(`[data-metric="${item.id}"]`); if(!node) continue;
        node.querySelector("span").textContent=item.title;
        node.querySelector("strong").textContent=item.label;
        node.querySelector("strong").className=item.id==="voltage" && snapshot.fields["seconds.voltage_low"]?.value===1 ? "bad" : "";
        node.querySelector("small").textContent=Number.isFinite(item.updated_ms) ? item.updated_ms+" мс от старта" : "Нет выборки";
      }
    }
  };
})();
