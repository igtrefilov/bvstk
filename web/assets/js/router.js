export function initRouter() {
  const tabs = ["home", "diag", "settings"];
  const navLinks = Array.from(document.querySelectorAll("a[data-tab]"));
  const sections = new Map(tabs.map((t) => [t, document.getElementById("tab-" + t)]));

  function setActive(tab) {
    if (!sections.has(tab)) tab = "home";
    navLinks.forEach((a) => a.classList.toggle("active", a.dataset.tab === tab));
    sections.forEach((el, key) => el.classList.toggle("hidden", key !== tab));
  }

  function onHash() {
    const raw = (location.hash || "#home").slice(1);
    setActive(raw);
  }

  window.addEventListener("hashchange", onHash);
  onHash();
}

