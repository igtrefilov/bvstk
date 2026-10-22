import * as api from "./api.js";

export function bundlePath(bus, fileName, asset) {
  const slash = typeof fileName === "string" ? fileName.lastIndexOf("/") : -1;
  const directory = slash > 0 ? fileName.slice(0,slash) : "";
  const leaf = slash > 0 ? fileName.slice(slash + 1) : fileName;
  const components = directory.split("/");
  const nested = slash > 0 && components.length <= 5 && components.every(part => /^[a-z0-9_-]{1,31}$/i.test(part)) &&
    (leaf.toLowerCase() === "profile.json" || leaf.toLowerCase() === components.at(-1).toLowerCase() + ".json");
  const flat = slash < 0 && typeof fileName === "string" && /^[a-z0-9_.-]+\.json$/i.test(fileName);
  if (typeof bus !== "string" || !/^[a-z][a-z0-9_-]{0,30}$/.test(bus) ||
      typeof fileName !== "string" || fileName.length >= 224 ||
      (!nested && !flat) || fileName.includes("..") ||
      typeof asset !== "string" || asset.length >= 64 || !/^[a-zA-Z0-9_-]+\.(json|js)$/.test(asset)) throw Error("Неверный путь пакета устройства");
  return `/config/${bus}/${slash < 0 ? "" : fileName.slice(0,slash+1)}${asset}`;
}

export async function readBundleAsset(bus, fileName, asset, signal, limit = 65536) {
  const value = await api.request(api.fileUrl("flash",bundlePath(bus,fileName,asset),limit),{signal,raw:true});
  if (new TextEncoder().encode(value).length > limit) throw Error("Файл пакета превышает допустимый размер");
  return value;
}
