export function byId(id) {
  return document.getElementById(id);
}

export function setText(id, value) {
  const el = byId(id);
  if (!el) return;
  el.textContent = value ?? "—";
}

export function setDot(id, state) {
  const el = byId(id);
  if (!el) return;
  el.classList.remove("ok", "bad");
  if (state === "ok") el.classList.add("ok");
  else if (state === "bad") el.classList.add("bad");
}

