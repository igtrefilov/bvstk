import {icon, esc, deviceState} from "./ui.js";

// Rebuild only when the topology changes. Status updates retain anchors,
// keyboard focus, expanded branches and the sidebar's scroll position.
export function updateDeviceTree(tree, bus, devices) {
  const signature = JSON.stringify(devices.map(d => [d.name,d.topology || null]));
  if (tree.dataset.topology !== signature) {
    const open = new Set([...tree.querySelectorAll("details[open]")].map(el => el.dataset.branch));
    const links = new Map([...tree.querySelectorAll("a[data-device-name]")].map(el => [el.dataset.deviceName,el]));
    const fragment = document.createDocumentFragment();
    const visited = new Set();
    function branch(parent, key, label, depth, kind) {
      const node = document.createElement("details"), heading = document.createElement("summary");
      node.className = `topology-branch topology-${kind}`;
      node.dataset.branch = key; node.open = open.has(key);
      heading.style.setProperty("--tree-depth",depth);
      if (label) heading.innerHTML = `${icon("bus")}<span class="device-label">${esc(label)}</span>`;
      node.append(heading); parent.append(node);
      return node;
    }
    function append(parent, device, depth = 0) {
      if (visited.has(device.name)) return;
      visited.add(device.name);
      const link = links.get(device.name) || document.createElement("a");
      link.href = `#${bus}?name=${encodeURIComponent(device.name)}`;
      link.dataset.deviceBus = bus; link.dataset.deviceName = device.name;
      link.style.setProperty("--tree-depth",depth);
      if (device.topology?.channels) {
        const sw = branch(parent,"device:"+device.name,"",depth,"switch");
        sw.querySelector("summary").append(link);
        for (let channel = 0; channel < device.topology.channels; channel++) {
          const ch = branch(sw,device.name+":"+channel,`Канал ${channel}`,depth+1,"channel");
          const children = devices.filter(d => d.topology?.parent === device.name && d.topology.channel === channel);
          for (const child of children) append(ch,child,depth+2);
          if (!children.length) {
            const empty = document.createElement("small");
            empty.style.setProperty("--tree-depth",depth+2);
            empty.className = "topology-empty"; empty.textContent = "Нет профилей"; ch.append(empty);
          }
        }
      } else parent.append(link);
    }
    for (const device of devices.filter(d => !d.topology?.parent)) append(fragment,device);
    // Keep a malformed/unresolved API entry visible rather than silently hiding it.
    for (const device of devices) append(fragment,device);
    tree.replaceChildren(fragment); tree.dataset.topology = signature;
  }
  const items = new Map(devices.map(d => [d.name,d]));
  for (const link of tree.querySelectorAll("a[data-device-name]")) {
    const device = items.get(link.dataset.deviceName);
    if (!device) continue;
    const state = deviceState(bus,device), ready = String(state.ready);
    const label = `${device.name}: ${state.label}`;
    if (link.dataset.deviceReady !== ready || !link.firstChild) {
      link.innerHTML = `${icon("chip",{ready:state.ready})}<span class="device-label">${esc(device.name)}</span>`;
      link.dataset.deviceReady = ready;
    }
    link.title = label; link.setAttribute("aria-label",label);
  }
}
