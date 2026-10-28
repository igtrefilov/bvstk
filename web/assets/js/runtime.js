import {bytes, uptime} from "./ui.js";
export const osName = runtime => ({freertos:"FreeRTOS",neutrino:"Нейтрино"}[runtime?.os] || "Неизвестно");
export function runtimeRows(runtime) {
  const details=runtime.os_details || {};
  const rows=[["Система",osName(runtime)],["Время работы",uptime(runtime.uptime_ms)]];
  if (runtime.os === "freertos") {
    if (details.heap_free != null) rows.push(["Свободная память",bytes(details.heap_free)]);
    if (details.heap_min_ever != null) rows.push(["Минимум свободной памяти",bytes(details.heap_min_ever)]);
    if (details.tick_rate_hz != null) rows.push(["Частота тиков",details.tick_rate_hz+" Гц"]);
  } else {
    if (details.pid != null) rows.push(["PID сервера",details.pid]);
    if (details.online_cpus != null) rows.push(["Процессоры",details.online_cpus]);
  }
  return rows;
}
