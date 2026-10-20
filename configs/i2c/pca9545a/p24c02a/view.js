// A sparse memory view: bytes absent from the physical cache stay unknown.
(() => {
  let root;
  const hex=n=>n.toString(16).toUpperCase().padStart(2,"0");
  window.bvstkView={
    apiVersion:1,
    mount(target) {
      root=target;
      root.innerHTML=`<style>
        .memory{overflow:auto}table{border-collapse:collapse;width:100%;font:12px monospace}td,th{padding:7px;border:1px solid #d7dfe4;text-align:center}
        th{color:#586874;background:#f6f8f9}td{color:#7b858d}.known{color:#24313a;background:#edf5ef}p{font-size:12px;color:#68737c}
      </style><p data-count></p><div class="memory"><table><thead><tr><th>Адрес</th>${Array.from({length:16},(_,i)=>`<th>${i.toString(16).toUpperCase()}</th>`).join("")}</tr></thead><tbody>
      ${Array.from({length:16},(_,row)=>`<tr><th>0x${hex(row*16)}</th>${Array.from({length:16},(_,col)=>`<td data-byte="${row*16+col}">—</td>`).join("")}</tr>`).join("")}
      </tbody></table></div><p>256 байт. Серые ячейки ещё не прочитаны или потеряли достоверность. Наведение на байт показывает метку времени. Страница только отображает кэш.</p>`;
    },
    update(snapshot) {
      let count=0;
      for(let i=0;i<256;i++) {
        const value=snapshot.fields["b"+hex(i).toLowerCase()+".v"], node=root.querySelector(`[data-byte="${i}"]`);
        const known=value?.status==="known";
        node.textContent=known?hex(value.value):"—"; node.className=known?"known":"";
        node.title=known ? "0x"+hex(i)+": "+value.updated_ms+" мс от старта" : "Нет данных";
        if(known) count++;
      }
      root.querySelector("[data-count]").textContent=`Известно ${count} из 256 байт`;
    }
  };
})();
