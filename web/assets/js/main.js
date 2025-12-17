import { initRouter } from "./router.js";
import { loadHomeTiles } from "./sections/home.js";
import { initI2cSettings } from "./sections/i2c_settings.js";
import { initSettings } from "./sections/settings.js";

initRouter();
initSettings();
initI2cSettings(loadHomeTiles);
loadHomeTiles();
