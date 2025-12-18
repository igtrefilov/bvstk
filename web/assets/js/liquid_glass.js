function clampPct(v) {
  if (!Number.isFinite(v)) return 50;
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

export function initLiquidGlassTiles() {
  if (typeof window === "undefined") return;
  if (!("requestAnimationFrame" in window)) return;
  if (window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;

  let activeCard = null;
  let pointerX = 0;
  let pointerY = 0;
  let rafId = 0;

  function setActive(next) {
    if (activeCard === next) return;
    if (activeCard) {
      activeCard.classList.remove("is-active");
      activeCard.style.removeProperty("--lg-x");
      activeCard.style.removeProperty("--lg-y");
      activeCard.style.removeProperty("--lg-h");
    }
    activeCard = next;
    if (activeCard) activeCard.classList.add("is-active");
  }

  function scheduleUpdate() {
    if (!activeCard) return;
    if (rafId) return;
    rafId = requestAnimationFrame(() => {
      rafId = 0;
      if (!activeCard) return;

      const rect = activeCard.getBoundingClientRect();
      const mx = clampPct(((pointerX - rect.left) / rect.width) * 100);
      const my = clampPct(((pointerY - rect.top) / rect.height) * 100);
      const h = ((window.scrollY || 0) * 0.18) % 360;

      activeCard.style.setProperty("--lg-x", `${mx}%`);
      activeCard.style.setProperty("--lg-y", `${my}%`);
      activeCard.style.setProperty("--lg-h", `${h}deg`);
    });
  }

  function findCardFromEventTarget(t) {
    if (!(t instanceof Element)) return null;
    return t.closest(".tiles .card");
  }

  document.addEventListener(
    "pointermove",
    (e) => {
      pointerX = e.clientX;
      pointerY = e.clientY;
      const next = findCardFromEventTarget(e.target);
      setActive(next);
      scheduleUpdate();
    },
    { passive: true }
  );

  document.addEventListener(
    "pointerdown",
    (e) => {
      pointerX = e.clientX;
      pointerY = e.clientY;
      const next = findCardFromEventTarget(e.target);
      setActive(next);
      scheduleUpdate();
    },
    { passive: true }
  );

  window.addEventListener(
    "scroll",
    () => {
      if (!activeCard) return;
      scheduleUpdate();
    },
    { passive: true }
  );

  document.addEventListener(
    "pointerout",
    (e) => {
      if (e.relatedTarget) return;
      setActive(null);
    },
    { passive: true }
  );

  window.addEventListener(
    "blur",
    () => {
      setActive(null);
    },
    { passive: true }
  );

  document.addEventListener(
    "focusin",
    (e) => {
      const next = findCardFromEventTarget(e.target);
      if (!next) return;
      setActive(next);
      const rect = next.getBoundingClientRect();
      pointerX = rect.left + rect.width * 0.5;
      pointerY = rect.top + rect.height * 0.35;
      scheduleUpdate();
    },
    { passive: true }
  );

  document.addEventListener(
    "focusout",
    (e) => {
      if (!activeCard) return;
      const rt = e.relatedTarget;
      if (rt instanceof Element && activeCard.contains(rt)) return;
      setActive(null);
    },
    { passive: true }
  );
}
