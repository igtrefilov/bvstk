// PCA9555 pin states from cached register values.
(() => {
  let root;
  window.bvstkView={
    apiVersion:1,
    mount(target) {
      root=target;
      root.innerHTML=`<style>
        .ports{display:grid;grid-template-columns:1fr 1fr;gap:16px}.port{overflow:auto}
        table{border-collapse:collapse;width:100%;font-size:12px}td,th{padding:9px;text-align:left;border-bottom:1px solid #d7dfe4}
        th{color:#586874}h3{font-size:14px}p{font-size:12px;line-height:1.5;color:#68737c}
        @media(max-width:850px){.ports{grid-template-columns:1fr}}
      </style><div class="ports">${[0,1].map(p=>`<section class="port"><h3>Порт ${p}</h3><table><thead><tr><th>Линия</th><th>Вход</th><th>Защёлка</th><th>Направление</th><th>Инверсия</th></tr></thead><tbody>
      ${Array.from({length:8},(_,b)=>`<tr data-pin="${p}_${b}"><td data-label>IO${p}_${b}</td>${["input","output","config","polarity"].map(group=>`<td data-field="${group}${p}.p${p}_${b}">Нет данных</td>`).join("")}</tr>`).join("")}
      </tbody></table></section>`).join("")}</div><p>Уровень Input учитывает Polarity. Защёлка Output действует при настроенном выходе. Данные разных столбцов могут быть считаны в разное время; метки доступны при наведении.</p>`;
    },
    update(snapshot) {
      for(let p=0;p<2;p++) for(let b=0;b<8;b++) {
        const line=snapshot.map.registers.find(r=>r.id==="input"+p)?.fields.find(f=>f.id===`p${p}_${b}`);
        root.querySelector(`[data-pin="${p}_${b}"] [data-label]`).textContent=line?.label || `IO${p}_${b}`;
      }
      for(const node of root.querySelectorAll("[data-field]")) {
        const value=snapshot.fields[node.dataset.field];
        node.textContent=value?.label || "Нет данных";
        node.title=Number.isFinite(value?.updated_ms)?value.updated_ms+" мс от старта":"Нет выборки";
      }
    }
  };
})();
