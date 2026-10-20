// Snapshot-only BVSTK view API 1. No physical I/O.
(() => {
  let root;
  window.bvstkView = {
    apiVersion: 1,
    mount(target) {
      root = target;
      root.innerHTML = `<style>
        table{border-collapse:collapse;width:100%;font-size:13px}th,td{padding:12px;text-align:left;border-bottom:1px solid #d7dfe4}
        th{color:#586874}td[data-state]{font-weight:600}.on{color:#247840}.irq{color:#a26616}p{color:#68737c;font-size:12px}
      </style><table><thead><tr><th>Канал</th><th>Маска</th><th>Прерывание</th></tr></thead><tbody>
        ${Array.from({length:4},(_,i)=>`<tr><td>${i}</td><td data-state="${i}">Нет данных</td><td data-irq="${i}">Нет данных</td></tr>`).join("")}
      </tbody></table><p data-time></p><p>Показано последнее чтение управляющего байта. После записи маски полный байт кэша ожидает нового чтения.</p>`;
    },
    update(snapshot) {
      for (let i=0;i<4;i++) {
        const state = snapshot.fields["control.ch"+i], irq = snapshot.fields["control.int"+i];
        const a=root.querySelector(`[data-state="${i}"]`), b=root.querySelector(`[data-irq="${i}"]`);
        a.textContent=state?.label || "Нет данных"; b.textContent=irq?.label || "Нет данных";
        a.className=state?.status==="known" && state.value===1 ? "on" : "";
        b.className=irq?.status==="known" && irq.value===1 ? "irq" : "";
      }
      const t=snapshot.fields["control.ch0"]?.updated_ms;
      root.querySelector("[data-time]").textContent="Метка времени: "+(Number.isFinite(t) ? t+" мс от старта" : "нет выборки");
    }
  };
})();
