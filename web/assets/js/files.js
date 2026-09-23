import * as api from "./api.js";
import {esc, icon, bytes, head, kv, badge, footer, ask, notice, action, wireRows} from "./ui.js";
import {volumePurpose} from "./overview.js";
const join = (parent,name) => (parent === "/" ? "/" : parent + "/") + name;
function validName(name) {
  if (!name || new TextEncoder().encode(name).length > 63 || /[\x00-\x1f\x7f:/\\*?"<>|]/.test(name) || /[ .]$/.test(name) || name === "." || name === "..") throw new Error("Недопустимое имя файла или каталога; предел — 63 байта UTF-8");
  return name;
}
function fatDate(d, t) {
  if (!d) return "Не задано";
  return `${String(d & 31).padStart(2,"0")}.${String((d >> 5) & 15).padStart(2,"0")}.${1980 + (d >> 9)} ${String(t >> 11).padStart(2,"0")}:${String((t >> 5) & 63).padStart(2,"0")}`;
}
export async function filesPage(root, params, ctx) {
  let volume = params.get("volume") || "sd-pl", path = params.get("path") || "/";
  let vols, entries = [], next = null, selected = null, filter = "", busy = false, fresh = "";
  let preview = null, previewTicket = 0, detailTab = "preview";
  const textFile = e => e && !e.directory && /\.(json|txt|log|csv|md|ini|conf|cfg|xml|html|css|js|tcl|sh)$/i.test(e.name) && e.size <= 32768;
  const sensitive = p => volume === "flash" && /^\/(config|www)(\/|$)/i.test(p);
  function navigate(v,p) { if (!busy) location.hash = "files?" + new URLSearchParams({volume:v,path:p}); }
  async function refresh() {
    vols = (await api.volumes(ctx.signal)).volumes;
    if (!vols.some(v => v.name === volume)) throw new Error("Неизвестный том");
    const current = vols.find(v => v.name === volume);
    if (current.ready) {
      const data = await api.fsList(volume,path,0,ctx.signal); entries = data.entries; next = data.next;
    } else { entries = []; next = null; }
    if (ctx.signal.aborted) return;
    if (!entries.some(e => e.name === selected)) selected = null;
    preview = null; previewTicket++; root.classList.remove("is-stale"); fresh = footer(); render();
    if (selected) loadPreview();
  }
  function table() {
    const rows = entries.filter(e => e.name.toLocaleLowerCase().includes(filter.toLocaleLowerCase())).sort((a,b) => Number(b.directory)-Number(a.directory) || a.name.localeCompare(b.name,"ru"));
    return `<div class="table-scroll"><table><thead><tr><th>Имя</th><th>Тип</th><th>Размер</th><th>Изменён</th><th></th></tr></thead><tbody>${rows.map(e => `<tr class="selectable ${selected === e.name ? "selected" : ""}" tabindex="0" data-entry="${esc(e.name)}"><td><button class="file-name text-button" data-open="${esc(e.name)}">${icon(e.directory ? "folder" : "file")}${esc(e.name)}</button></td><td>${e.directory ? "Каталог" : esc(e.name.includes(".") ? e.name.split(".").pop().toUpperCase() : "Файл")}</td><td>${e.directory ? "—" : bytes(e.size)}</td><td>${e.date ? esc(fatDate(e.date,e.time)) : "—"}</td><td><button class="text-button" data-menu="${esc(e.name)}" aria-label="Свойства ${esc(e.name)}">⋯</button></td></tr>`).join("") || '<tr><td colspan="5" class="empty">Нет объектов</td></tr>'}</tbody></table></div><div class="file-count">Показано ${rows.length}, загружено ${entries.length}${next != null ? ", есть ещё" : ""}. ${selected ? "Выбран один объект" : ""}</div>${next != null ? '<p><button data-more>Показать ещё</button></p>' : ""}`;
  }
  function detail() {
    const e = entries.find(e => e.name === selected);
    if (!e) return '<div class="empty">Выберите файл или каталог</div>';
    const text = textFile(e);
    return `<h3 class="file-name">${icon(e.directory ? "folder" : "file")}${esc(e.name)}</h3><div class="tabs compact">${text ? `<button data-detail-tab="preview" class="${detailTab === "preview" ? "active" : ""}">Просмотр</button>` : ""}<button data-detail-tab="properties" class="${!text || detailTab === "properties" ? "active" : ""}">Свойства</button></div>${text && detailTab === "preview" ? `<div id="file-preview">${preview?.error ? `<p class="error-text">${esc(preview.error)}</p>` : preview?.content != null ? `<pre class="file-preview">${esc(preview.content)}</pre>` : '<div class="empty">Чтение файла…</div>'}</div><div class="actions"><button data-edit-text>Редактировать текст</button></div><p><small>Предпросмотр до 32 КиБ</small></p>` : '<div style="height:14px"></div>'}${kv([["Путь",volume + ":" + join(path,e.name)],["Размер",e.directory ? "—" : bytes(e.size)],["Изменён",fatDate(e.date,e.time)]])}${e.directory ? '<div class="actions"><button data-enter>Открыть каталог</button></div>' : ""}<p class="muted">${sensitive(join(path,e.name)) ? "Системный каталог: изменения могут повлиять на работу bvstk." : "Изменение файла требует подтверждения."}</p>`;
  }
  async function loadPreview() {
    const e = entries.find(e => e.name === selected);
    if (!textFile(e) || detailTab !== "preview" || preview) return;
    const ticket = ++previewTicket, name = selected;
    try {
      const content = await api.request(api.fileUrl(volume,join(path,name),32768),{raw:true,signal:ctx.signal,timeout:60000});
      if (new TextEncoder().encode(content).length > 32768) throw new Error("Предел предпросмотра — 32 КиБ");
      if (ctx.signal.aborted || ticket !== previewTicket || selected !== name) return;
      preview = {content};
    } catch(e) {
      if (ctx.signal.aborted || ticket !== previewTicket || selected !== name) return;
      preview = {error:e.message};
    }
    drawDetail();
  }
  function drawDetail() { root.querySelector("#file-detail").innerHTML = detail(); wireDetail(); updateToolbar(); }
  function updateToolbar() {
    const e = entries.find(e => e.name === selected);
    for (const name of ["download","rename","copy","remove"]) {
      const button = root.querySelector(`[data-${name}]`);
      if (button) button.disabled = !e || ((name === "download" || name === "copy") && e.directory);
    }
  }
  function render() {
    const current = vols.find(v => v.name === volume);
    ctx.setContext({volume,path});
    root.innerHTML = `${head("Файлы",volume + ":" + path)}<div class="file-toolbar actions"><button data-upload ${!current.ready ? "disabled" : ""}>${icon("upload")}Загрузить</button><button data-download disabled>${icon("download")}Скачать</button><button data-mkdir ${!current.ready ? "disabled" : ""}>${icon("folder")}Создать каталог</button><button data-rename disabled>${icon("edit")}Переименовать</button><button data-copy disabled>${icon("copy")}Копировать</button><button data-remove disabled>${icon("trash")}Удалить</button></div><div class="path-toolbar"><form class="path-form"><button type="button" data-up class="icon-button" ${path === "/" ? "disabled" : ""} aria-label="Родительский каталог">${icon("up")}</button><input aria-label="Путь к каталогу" value="${esc(volume+":"+path)}" autocomplete="off"></form><input class="filter" aria-label="Фильтр по имени" placeholder="Фильтр загруженных имён" value="${esc(filter)}"></div><div class="volume-strip"><span>${esc(volumePurpose(volume))}</span>${badge(current.ready ? "Смонтирован" : "Недоступен",current.ready ? "ok" : "bad")}<span>${current.fat_bits ? "FAT"+current.fat_bits : "Тип ФС не определён"}</span><span>Размер тома ${bytes(current.total_bytes)}</span><span>Свободно ${current.free_bytes == null ? "не определено" : bytes(current.free_bytes)}</span></div><input type="file" id="upload-file" class="hidden">${current.ready ? `<div class="file-layout"><div class="file-list" id="file-table">${table()}</div><aside class="card file-detail" id="file-detail">${detail()}</aside></div>` : '<article class="card"><div class="empty">Том недоступен. Проверьте состояние носителя и сообщения системы.</div></article>'}${fresh}`;
    root.querySelector("[data-refresh]").onclick = () => ctx.runRefresh(refresh);
    root.querySelector(".path-form").onsubmit = event => {
      event.preventDefault(); const value = event.currentTarget.querySelector("input").value.trim();
      const match = /^(flash|sd|sd-pl):(\/.*)$/.exec(value);
      if (match) navigate(match[1],match[2]);
      else if (value.startsWith("/")) navigate(volume,value);
      else notice("Укажите путь вида sd-pl:/каталог или /каталог",true);
    };
    root.querySelector("[data-up]").onclick = () => navigate(volume,path.slice(0,path.lastIndexOf("/")) || "/");
    const filterInput = root.querySelector(".filter");
    if (filterInput) { filterInput.disabled = !current.ready; filterInput.oninput = () => { filter = filterInput.value; redrawTable(); }; }
    root.querySelector("[data-upload]")?.addEventListener("click", () => root.querySelector("#upload-file").click());
    root.querySelector("#upload-file")?.addEventListener("change", event => {
      const file = event.target.files[0]; event.target.value = "";
      if (file) action(null, () => sendFile(file));
    });
    root.querySelector("[data-mkdir]")?.addEventListener("click", () => ask({title:"Создать каталог",body:'<label>Имя каталога<input name="name" required maxlength="63"></label>',submit:"Создать",run:async form => {
      const p = join(path,validName(form.elements.name.value));
      await operation(() => api.fsAction(volume,p,"mkdir"));
    }}));
    if (current.ready) { wireTable(); wireDetail(); updateToolbar(); }
  }
  function redrawTable() { root.querySelector("#file-table").innerHTML = table(); wireTable(); }
  function select(name) {
    selected = name; preview = null; previewTicket++; detailTab = "preview";
    redrawTable(); drawDetail(); loadPreview();
  }
  function wireTable() {
    wireRows(root,"[data-entry]",row => select(row.dataset.entry));
    root.querySelectorAll("[data-open]").forEach(b => b.onclick = event => {
      event.stopPropagation(); const e = entries.find(e => e.name === b.dataset.open);
      if (e.directory) navigate(volume,join(path,e.name)); else select(e.name);
    });
    root.querySelectorAll("[data-menu]").forEach(b => b.onclick = event => { event.stopPropagation(); select(b.dataset.menu); });
    root.querySelector("[data-more]")?.addEventListener("click", event => action(event.currentTarget,async () => {
      const data = await api.fsList(volume,path,next,ctx.signal);
      const names = new Set(entries.map(e => e.name)); entries.push(...data.entries.filter(e => !names.has(e.name))); next = data.next;
      redrawTable();
    }));
  }
  async function operation(work) {
    busy = true;
    try {
      await work();
      try { await refresh(); }
      catch (e) { notice("Операция выполнена, но список не обновлён. Обновите его вручную. " + e.message,true); }
    } finally { busy = false; }
  }
  async function sendFile(file) {
    validName(file.name);
    const p = join(path,file.name); let exists = false;
    try { const item = await api.fsStat(volume,p); if (item.directory) throw new Error("Каталог с таким именем уже существует"); exists = true; }
    catch(e) { if (e.status !== 404) throw e; }
    await ask({title:exists ? "Перезаписать файл?" : "Загрузить файл?",submit:exists ? "Перезаписать" : "Загрузить",danger:exists,
      body:`<p class="mono wrap-text">${esc(volume + ":" + p)}</p><p>${bytes(file.size)}. ${exists ? "Прежнее содержимое будет заменено." : "Будет создан новый файл."} ${sensitive(p) ? "Это системный каталог." : ""}</p><progress class="progress" value="0" max="100"></progress><small id="upload-state">Ожидание отправки</small>`,run:async form => operation(async () => {
        await api.upload(volume,p,file,exists,(loaded,total) => { form.querySelector("progress").value = loaded * 100 / total; form.querySelector("#upload-state").textContent = loaded === total ? "Данные отправлены. Ожидание подтверждения записи…" : `${bytes(loaded)} из ${bytes(total)}`; });
        notice("Файл записан, закрытие подтверждено сервером");
      })});
  }
  function wireDetail() {
    const e = entries.find(e => e.name === selected); if (!e) return;
    const p = join(path,e.name);
    root.querySelectorAll("[data-detail-tab]").forEach(button => button.onclick = () => { detailTab = button.dataset.detailTab; drawDetail(); loadPreview(); });
    root.querySelector("[data-enter]")?.addEventListener("click", () => navigate(volume,p));
    root.querySelector("[data-download]").onclick = () => {
      api.pausePolling(); const a = document.createElement("a"); a.href = api.fileUrl(volume,p); a.download = e.name; document.body.append(a); a.click(); a.remove();
      notice("Скачивание передано браузеру. Автоопрос приостановлен на время передачи.");
    };
    root.querySelector("[data-edit-text]")?.addEventListener("click", event => action(event.currentTarget,async () => {
      const text = await api.request(api.fileUrl(volume,p,32768),{raw:true,signal:ctx.signal,timeout:60000});
      if (new TextEncoder().encode(text).length > 32768) throw new Error("Файл слишком большой для редактора");
      await ask({title:e.name,submit:"Записать изменения",body:`<label>Содержимое<textarea class="file-editor mono" name="content" spellcheck="false">${esc(text)}</textarea></label><label class="check"><input type="checkbox" name="confirm" required>Подтверждаю перезапись файла</label><p class="muted">Редактирование файла не применяет конфигурацию к работающим подсистемам. ${sensitive(p) ? "Изменяется системный файл." : ""}</p>`,run:async form => {
        const current = await api.request(api.fileUrl(volume,p,32768),{raw:true,signal:ctx.signal,timeout:60000});
        if (current !== text) throw new Error("Файл изменён другим клиентом. Закройте редактор и загрузите его заново.");
        const blob = new Blob([form.elements.content.value],{type:"text/plain;charset=utf-8"});
        if (blob.size > 32768) throw new Error("Предел редактора — 32 КиБ");
        await operation(() => api.upload(volume,p,blob,true)); notice("Файл сохранён");
      }});
    }));
    root.querySelector("[data-remove]") .onclick = () => ask({title:e.directory ? "Удалить пустой каталог?" : "Удалить файл?",body:`<p class="mono">${esc(volume + ":" + p)}</p><p>Операцию нельзя отменить.${e.directory ? " Непустые каталоги не удаляются." : ""}${sensitive(p) ? " Это системный каталог." : ""}</p>`,submit:"Удалить",danger:true,run:() => operation(() => api.fsAction(volume,p,"remove"))});
    root.querySelector("[data-rename]") .onclick = () => ask({title:"Переименовать",body:`<label>Новое имя<input name="name" value="${esc(e.name)}" required maxlength="63"></label><p class="muted">Существующий объект не будет перезаписан.</p>`,submit:"Переименовать",run:form => operation(() => api.fsAction(volume,p,"rename",{dst_volume:volume,dst:join(path,validName(form.elements.name.value))}))});
    root.querySelector("[data-copy]") .onclick = () => ask({title:"Копировать файл",body:`<label>Том<select name="volume">${vols.filter(v => v.ready).map(v => `<option ${v.name === volume ? "selected" : ""} value="${v.name}">${v.name}:/</option>`).join("")}</select></label><label>Полный путь на томе<input name="path" value="${esc(join(path,e.name + ".copy"))}" required></label><p class="muted">Промежуточные каталоги должны существовать. Перезапись запрещена.</p>`,submit:"Копировать",run:form => operation(() => api.fsAction(volume,p,"copy",{dst_volume:form.elements.volume.value,dst:form.elements.path.value}))});
  }
  await refresh(); return {refresh,dirty:() => busy,busy:() => busy};
}
