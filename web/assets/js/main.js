import { initRouter } from "./router.js";
import { loadHomeTiles } from "./sections/home.js";
import { initSettings } from "./sections/settings.js";

initRouter();
initSettings();
loadHomeTiles();

