#!/usr/bin/env python3
"""Generate the explanatory figures outside the established PL series.

Only the Python standard library is required. Layouts describe actual paths;
files, tools and history records deliberately have shapes distinct from runtime
components. Layer colours and OS boundaries match the L0–L4 architecture maps.
"""
from pathlib import Path
import json
import xml.etree.ElementTree as ET

import generate_pl_subsystem as drawing
from generate_pl_subsystem import Page, Box, el, PALETTE, ARROWS, NS
from transaction_drawing import owned

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/figures"
ARROWS.update({
    "build": ("#788794", "Вход сборки", None),
    "depends": ("#788794", "Зависимость кода", "4 5"),
})


class Figure(Page):
    def __init__(self, name, title, sources=(), width=1800):
        super().__init__(name, title, title, sources)
        self.width = width
        self.root.set("width", str(width))
        self.background.set("width", str(width))
        self.legend_note = ""

    def artifact(self, key, x, y, w, h, label, detail=""):
        """A folded sheet denotes data or a file, never an abstraction layer."""
        g = el(self.blocks, "g", id=key, data_kind="artifact")
        el(g, "path", d=f"M{x},{y} H{x+w-28} L{x+w},{y+28} V{y+h} H{x} Z",
           fill="white", stroke="#607487", stroke_width=2)
        el(g, "path", d=f"M{x+w-28},{y} V{y+28} H{x+w}",
           fill="none", stroke="#607487", stroke_width=2)
        self.text(g, x+22, y+48, label, 24, bold=True, width=w-50)
        if detail:
            self.text(g, x+22, y+87, detail, 19, width=w-44)
        self.nodes[key] = Box(key,x,y,w,h,g,label)
        self.min_y = min(self.min_y,y)
        self.max_y = max(self.max_y,y+h)

    def tool(self, key, x, y, w, h, label):
        g = el(self.blocks, "g", id=key, data_kind="build-tool")
        el(g,"rect",x=x,y=y,width=w,height=h,rx=14,
           fill="#f7f9fb",stroke="#607487",stroke_width=2)
        self.text(g,x+w/2,y+h/2+9,label,24,bold=True,anchor="middle")
        self.nodes[key]=Box(key,x,y,w,h,g,label)
        self.min_y=min(self.min_y,y);self.max_y=max(self.max_y,y+h)

    def save(self):
        super().save()
        path=OUT/self.name
        tree=ET.parse(path)
        metadata=tree.getroot().find(f"{{{NS}}}metadata")
        data=json.loads(metadata.text)
        data["generator"]="tools/docs/generate_documentation.py"
        metadata.text=json.dumps(data,ensure_ascii=False,indent=2)
        tree.write(path,encoding="utf-8",xml_declaration=True)


def startup():
    p=Figure("system/runtime/startup.svg","Запуск рабочей среды",
             ["src/apps/freertos/main.c","src/apps/freertos/runtime/bvstk_runtime.c",
              "src/apps/neutrino/bvstkd/main.c"])
    owned(p,"freertos",60,100,530,1040,"FreeRTOS")
    owned(p,"neutrino",1250,100,490,660,"Нейтрино")
    p.node("main",110,190,430,110,"main",layer="L4",parent="freertos")
    p.node("tasks",110,390,430,150,"Задачи носителей\nи config_store",layer="L4",parent="freertos")
    p.node("runtime",110,630,430,110,"bvstk_runtime",layer="L4",parent="freertos")
    p.node("sd",110,950,430,120,"Запуск SD через PL",layer="L4",parent="freertos")
    p.node("ifs",1300,190,390,110,"Сценарий IFS",layer="L4",parent="neutrino")
    p.node("ntfs",1300,390,390,130,"Файловые менеджеры",layer="L3",parent="neutrino")
    p.node("daemon",1300,630,390,110,"bvstkd",layer="L4",parent="neutrino")
    p.node("i2c",710,615,420,140,"Сервисы I²C",layer="L3")
    p.node("mdio",710,825,420,140,"Сервис MDIO",layer="L3")
    p.edge("main","tasks","call",start="S",end="N",label="Создание задач",label_xy=(360,352))
    p.edge("tasks","runtime","data",start="S",end="N",label="Профили готовы",label_xy=(355,588))
    p.edge("runtime","i2c","call",start="E",end="W")
    p.path("runtime","mdio",((540,715),(645,715),(645,895),(710,895)),"call")
    p.edge("ifs","ntfs","call",start="S",end="N")
    p.edge("ntfs","daemon","data",start="S",end="N",label="Файлы доступны",label_xy=(1500,574))
    p.edge("daemon","i2c","call",start="W",end="E")
    p.path("main","sd",((110,245),(85,245),(85,1010),(110,1010)),"call")
    p.legend_note="Пунктир — принадлежность ОС. Создание задачи предшествует её готовности."
    p.save()


def ps_pl():
    p=Figure("system/platform/ps-pl.svg","Граница PS и PL",
             ["src/hardware/boards/ax7020/bvstk_hw_config.h","src/drivers/pl/sd/bvstk_sd_master.c"])
    p.frame("ps",60,100,440,740,"PS",None)
    p.frame("pl",570,100,1170,740,"PL", "PL")
    p.node("cpu",110,300,340,140,"Cortex-A9",layer="L0",parent="ps")
    p.node("gic",110,640,340,110,"GIC",layer="L0",parent="ps")
    p.node("regs",650,300,360,140,"Регистры ядер",parent="pl")
    p.node("bram",1260,300,370,140,"BRAM",parent="pl")
    p.frame("cores",650,600,460,180,"Ядра шин","PL",parent="pl")
    p.node("slave",675,670,165,80,"pl_slave",parent="cores")
    p.node("master",855,670,220,80,"pl_master",parent="cores")
    p.node("dma",1260,640,370,110,"AXI DMA",parent="pl")
    p.edge("cpu","regs","mmio",start="E",end="W")
    p.path("cpu","bram",((280,300),(280,230),(1445,230),(1445,300)),"data",bidirectional=True)
    p.edge("regs","cores","mmio",start="S",end="N",a_offset=50)
    p.edge("cores","gic","irq",start="W",end="E",a_offset=5)
    p.edge("gic","cpu","irq",start="N",end="S")
    p.edge("bram","dma","data",start="S",end="N",bidirectional=True)
    p.path("bram","cores",((1260,410),(1070,410),(1070,600)),"data",bidirectional=True)
    p.edge("dma","cores","data",start="W",end="E",bidirectional=True,a_offset=15,b_offset=20)
    p.legend_note="DMA относится к тракту SD. Остальные ядра используют свой регистровый контракт."
    p.save()


def irq():
    p=Figure("system/platform/i2c-irq.svg","Ведомый IRQ I²C в двух ОС",
             ["src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c",
              "src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"])
    owned(p,"freertos",60,100,470,660,"FreeRTOS")
    owned(p,"neutrino",1250,100,490,660,"Нейтрино")
    p.node("pl",680,175,430,120,"I²C pl_slave",layer="PL")
    p.node("isr",115,175,360,120,"ISR",layer="L1",parent="freertos")
    p.node("queue",115,365,360,120,"Очередь",layer="L2",parent="freertos")
    p.node("worker",115,595,360,110,"Задача обработки",layer="L1",parent="freertos")
    p.node("wait",1300,175,390,120,"InterruptWait",layer="L2",parent="neutrino")
    p.node("thread",1300,585,390,130,"Поток IRQ",layer="L1",parent="neutrino")
    p.node("service",680,585,430,130,"Сервис I²C slave",layer="L3")
    p.node("reply",680,875,430,120,"Драйвер: ответ BRAM",layer="L3")
    p.edge("pl","isr","irq",start="W",end="E")
    p.edge("pl","wait","irq",start="E",end="W")
    p.edge("isr","queue","irq",start="S",end="N")
    p.edge("queue","worker","irq",start="S",end="N")
    p.edge("wait","thread","irq",start="S",end="N")
    p.edge("worker","service","call",start="E",end="W")
    p.edge("thread","service","call",start="W",end="E")
    p.edge("service","reply","call",start="S",end="N")
    p.path("reply","pl",((1110,935),(1180,935),(1180,345),(1000,345),(1000,295)),"mmio")
    p.legend_note="Пунктирная область — код ОС. Подтверждение PL и разрешение IRQ выполняет адаптер."
    p.save()


def configuration():
    p=Figure("system/configuration/state.svg","Изменение состояния конфигурации",
             ["src/apps/freertos/config/config_store.c","src/apps/freertos/services/http/http_ui_bus.c"])
    p.artifact("file",70,240,400,150,"Профиль JSON","Сохранённые настройки")
    p.node("service",710,240,450,150,"Сервис\nполитика и кэш",layer="L3")
    p.node("editor",710,660,450,130,"Редактор\nчерновик",layer="L4")
    p.node("device",1430,240,300,150,"Устройство",layer="L0")
    p.edge("file","service","data","Загрузить",start="E",end="W",label_xy=(590,289))
    p.edge("service","device","data","Записать регистр",start="E",end="W",label_xy=(1295,289))
    p.edge("editor","service","call","Применить",start="N",end="S",label_xy=(1000,567))
    p.path("service","file",((770,390),(770,470),(270,470),(270,390)),"data","Сохранить",label_xy=(490,452))
    p.legend_note="Лист — сохранённые данные. Цвет блока — уровень абстракции."
    p.save()


def package():
    p=Figure("system/configuration/package.svg","Файлы пакета и их потребители",
             ["src/shared/config/bvstk_device_profile.c","web/assets/js/device-view.js"])
    p.frame("folder",60,100,570,780,"Папка устройства",None)
    p.artifact("profile",115,200,460,135,"<имя>.json","Адрес, политика, опрос")
    p.artifact("map",115,450,460,135,"register_map.json","Регистры и битовые поля")
    p.artifact("view",115,685,460,135,"view.js","Компоновка показателей")
    p.node("service",940,200,700,135,"Сервис PS",layer="L3")
    p.frame("browser",850,390,880,490,"Веб-интерфейс", "L4")
    p.node("decode",940,450,700,135,"Расшифровка кэша",layer="L4",parent="browser")
    p.node("form",940,685,700,135,"Индивидуальная форма",layer="L4",parent="browser")
    p.edge("profile","service","data",start="E",end="W")
    p.edge("map","decode","data",start="E",end="W")
    p.edge("view","form","data",start="E",end="W")
    p.edge("service","decode","data","Снимок кэша",start="S",end="N",label_xy=(1390,369))
    p.edge("decode","form","data",start="S",end="N")
    p.legend_note="Лист — файл. Карта и форма не выполняют операций на шине."
    p.save()


def volumes():
    p=Figure("system/storage/volumes.svg","Файловые пути и носители",
             ["src/apps/freertos/storage/fs/fs_devices.c","src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.c"])
    owned(p,"freertos",60,100,520,880,"FreeRTOS")
    owned(p,"neutrino",1160,100,620,670,"Нейтрино")
    p.node("fatfs",100,190,430,120,"FatFs",layer="L3",parent="freertos")
    p.node("osfs",1210,190,460,120,"Файловый стек ОС",layer="L3",parent="neutrino")
    for key,y,label in [("qf",450,"flash:/"),("sf",650,"sd:/"),("pf",850,"sd-pl:/")]:
        p.node(key,250,y,280,110,label,layer="L1",parent="freertos")
    for key,y,label in [("qn",450,"/flash"),("sn",650,"/sd")]:
        p.node(key,1370,y,300,110,label,layer="L1",parent="neutrino")
    p.node("qspi",720,450,340,110,"QSPI PS",layer="L0")
    p.node("sdps",720,650,340,110,"Контроллер SD PS",layer="L0")
    p.node("plsvc",720,850,340,110,"Сервис SD",layer="L3")
    p.node("sdpl",720,1130,340,120,"SPI master и DMA",layer="PL")
    for key,x,y in [("qf",220,505),("sf",175,705),("pf",130,905)]:
        p.path("fatfs",key,((x,310),(x,y),(250,y)),"call")
    for key,x,start_y,end_y in [("qn",1705,240,505),("sn",1750,285,705)]:
        p.path("osfs",key,((1670,start_y),(x,start_y),(x,end_y),(1670,end_y)),"call")
    for a,b in [("qf","qspi"),("sf","sdps")]:
        p.edge(a,b,"mmio",start="E",end="W")
    for a,b in [("qn","qspi"),("sn","sdps")]:
        p.edge(a,b,"mmio",start="W",end="E")
    p.edge("pf","plsvc","call",start="E",end="W")
    p.edge("plsvc","sdpl","mmio",start="S",end="N")
    p.legend_note="Префикс выбирает порт носителя. Пунктир — принадлежность ОС."
    p.save()


def network():
    p=Figure("system/network/stacks.svg","Сетевые пути двух ОС",
             ["src/apps/freertos/services/lan/bvstk_lan.c","src/apps/neutrino/bvstkd/main.c"])
    owned(p,"freertos",60,100,770,620,"FreeRTOS")
    owned(p,"neutrino",980,100,760,620,"Нейтрино")
    p.node("servers",120,190,650,145,"Консоль, HTTP, DCP2\nSSH при включении",layer="L4",parent="freertos")
    p.node("lwip",220,510,450,125,"lwIP",layer="L3",parent="freertos")
    p.node("daemon",1040,190,300,145,"bvstkd\nDCP2",layer="L4",parent="neutrino")
    p.node("ssh",1410,190,270,145,"OpenSSH",layer="L4",parent="neutrino")
    p.node("stack",1090,510,540,125,"Сетевой стек ОС",layer="L3",parent="neutrino")
    p.node("eth",660,875,500,125,"Ethernet PS",layer="L0")
    p.edge("servers","lwip","call",start="S",end="N")
    p.edge("daemon","stack","call",start="S",end="N",b_offset=-170)
    p.edge("ssh","stack","call",start="S",end="N",b_offset=185)
    p.path("lwip","eth",((445,635),(445,800),(790,800),(790,875)),"data")
    p.path("stack","eth",((1360,635),(1360,800),(1030,800),(1030,875)),"data")
    p.legend_note="Пунктир — принадлежность ОС. Драйверы Ethernet опущены."
    p.save()


def requests():
    p=Figure("system/interfaces/requests.svg","Обработчики запросов FreeRTOS",
             ["src/apps/freertos/runtime/bvstk_runtime.c","src/apps/freertos/services/http/http_ui_bus.c",
              "src/apps/freertos/services/dcp2/dcp2_server.c"])
    owned(p,"freertos",60,100,540,740,"FreeRTOS")
    p.node("console",120,195,420,120,"shell",layer="L4",parent="freertos")
    p.node("http",120,425,420,120,"HTTP UI",layer="L4",parent="freertos")
    p.node("dcp",120,655,420,120,"Сервер DCP2",layer="L4",parent="freertos")
    p.frame("service",850,190,830,640,"Экземпляр сервиса устройства","L3")
    p.node("policy",930,285,660,125,"Проверка политики",layer="L3",parent="service")
    p.node("operation",930,590,350,145,"Операция",layer="L3",parent="service")
    p.node("cache",1380,590,210,145,"Кэш",layer="L3",parent="service")
    p.edge("console","service","call",start="E",end="W",b_offset=-255)
    p.edge("http","service","call",start="E",end="W",b_offset=-25)
    p.edge("dcp","service","call",start="E",end="W",b_offset=205)
    p.edge("policy","operation","call",start="S",end="N",a_offset=-155)
    p.edge("operation","cache","data",start="E",end="W")
    p.node("driver",930,1010,350,125,"Драйвер",layer="L3")
    p.node("pl",930,1320,350,125,"Ядро PL",layer="PL")
    p.edge("operation","driver","call",start="S",end="N")
    p.edge("driver","pl","mmio",start="S",end="N")
    p.legend_note="Показаны обычные операции устройств. MEM имеет отдельный контракт."
    p.save()


def dcp2():
    p=Figure("reference/dcp2/access.svg","Внешний клиент DCP2 и части BVSTK",
             ["src/apps/freertos/services/dcp2/dcp2_server.c",
              "src/protocols/dcp2/bvstk_dcp2_cache.c",
              "src/protocols/dcp2/bvstk_dcp2_fs.c"],width=2000)
    p.subtitle = ("Клиент подключается через Ethernet и TCP к серверу L4. "
                  "Команды устройств проходят через сервисы L3, MEM обращается "
                  "к разрешённым аппаратным адресам через L1. FS читает файлы. "
                  "Сервер возвращает результаты и события. Драйверы и порты ОС опущены.")
    p.root.find(f"{{{NS}}}desc").text=p.subtitle
    p.frame("bvstk",500,60,1460,1310,"BVSTK",None)
    p.tool("client",40,165,340,160,"Внешний клиент")
    p.node("server",590,170,1250,155,"Сервер DCP2",layer="L4",parent="bvstk")
    server_label=p.nodes["server"].group.findall(f"{{{NS}}}text")[-1]
    server_label.set("x","1215")
    server_label.set("text-anchor","middle")
    server_label.set("font-size","28")

    p.node("mem",625,575,290,140,"Доступ MMIO",layer="L1",parent="bvstk")
    p.frame("services",1015,505,510,400,"Сервисы I²C и MDIO","L3",parent="bvstk")
    p.node("policy",1050,590,440,95,"Политика записи",layer="L3",parent="services")
    p.node("cache",1050,755,205,100,"Кэш",layer="L3",parent="services")
    p.node("journal",1285,755,205,100,"Журналы",layer="L3",parent="services")
    p.node("fs",1680,575,210,140,"Файловый\nсервис",layer="L3",parent="bvstk")

    p.frame("pl",590,1080,960,230,"PL","PL",parent="bvstk")
    p.node("registers",625,1170,290,110,"Регистры\nи BRAM",parent="pl")
    p.node("cores",1100,1170,340,110,"Ядра шин",parent="pl")
    p.artifact("files",1655,1160,260,130,"Файлы")
    p.node("device",1100,1460,340,120,"Устройство",layer="L0")

    p.path("client","server",((380,210),(590,210)),"data",bidirectional=True)
    p.text(p.notes,440,188,"Ethernet",20,color=ARROWS["data"][0],anchor="middle")
    p.text(p.notes,545,188,"TCP",20,color=ARROWS["data"][0],anchor="middle")
    p.path("server","client",((590,280),(380,280)),"irq")
    p.text(p.notes,440,258,"События",20,color=ARROWS["irq"][0],anchor="middle")
    p.path("server","mem",((770,325),(770,575)),"call")
    p.text(p.notes,843,470,"MEM",24,color=ARROWS["call"][0],anchor="middle")
    p.path("server","services",((1130,325),(1130,505)),"call")
    p.path("services","server",((1460,505),(1460,325)),"irq")
    p.path("server","fs",((1785,325),(1785,575)),"call")
    p.text(p.notes,1847,470,"FS",24,color=ARROWS["call"][0],anchor="middle")
    p.path("mem","registers",((770,715),(770,1170)),"mmio")
    p.path("services","cores",((1270,905),(1270,1170)),"data",bidirectional=True)
    p.path("registers","cores",((915,1225),(1100,1225)),"data",bidirectional=True)
    p.path("files","fs",((1785,1160),(1785,715)),"data")
    p.text(p.notes,1860,970,"Чтение",24,color=ARROWS["data"][0],anchor="middle")
    p.path("cores","device",((1270,1280),(1270,1460)),"data",bidirectional=True)
    p.text(p.notes,1350,1403,"Шина",24,color=ARROWS["data"][0],anchor="middle")
    p.legend_note="Лист — сохранённые файлы. Слева показана программа вне BVSTK."
    p.save()


def observations():
    p=Figure("system/observation/paths.svg","Состояние и история операции",
             ["src/services/cache/bvstk_cache_bridges.c","src/apps/freertos/runtime/bvstk_observer.c"])
    p.node("service",660,120,470,135,"Сервис устройства",layer="L3")
    p.node("cache",100,475,570,150,"Кэш\nзначение, источник, время",layer="L3")
    p.node("journal",1110,475,570,150,"Журнал\nпопытка, отказ, результат",layer="L3")
    p.node("snapshot",100,870,570,125,"Снимок состояния",layer="L4")
    p.node("history",1110,870,570,125,"История с курсором",layer="L4")
    p.path("service","cache",((760,255),(760,365),(385,365),(385,475)),"data")
    p.path("service","journal",((1030,255),(1030,365),(1395,365),(1395,475)),"irq")
    p.edge("cache","snapshot","data",start="S",end="N")
    p.edge("journal","history","data",start="S",end="N")
    p.legend_note="Чтение снимка и истории не запускает физический обмен."
    p.save()


def history():
    p=Figure("system/observation/history.svg","Окно истории и курсор",
             ["src/shared/events/bvstk_journal.c"],width=1600)
    p.artifact("cursor",70,120,310,140,"Курсор: 96","Последняя позиция клиента")
    p.text(p.notes,80,355,"97–99",28,bold=True,color="#a44d41")
    p.text(p.notes,80,402,"Вытеснены",23,color="#a44d41")
    el(p.blocks,"rect",x=60,y=290,width=270,height=190,rx=12,fill="#fff5f2",stroke="#c65b4e",stroke_width=2,stroke_dasharray="9 6")
    p.frame("window",450,290,1070,260,"256 записей",None)
    for x,label in [(500,"100"),(700,"101"),(1000,"…"),(1260,"355")]:
        el(p.blocks,"rect",x=x,y=380,width=160,height=110,rx=8,fill="#f3f8ff",stroke="#3f75ad",stroke_width=2)
        p.text(p.notes,x+80,448,label,30,bold=True,anchor="middle")
    p.artifact("reply",605,745,760,150,"Ответ клиенту","lost = 3; чтение начинается с 100")
    p.path("cursor","window",((380,190),(580,190),(580,290)),"data")
    p.edge("window","reply","data",start="S",end="N",label="Доступная история",label_xy=(1050,657))
    p.legend_note="Номера условные. Ячейки обозначают записи журнала, а не программные модули."
    p.save()


def dependencies():
    p=Figure("development/dependencies.svg","Зависимости исходников")
    p.node("apps",660,100,470,125,"apps",layer="L4")
    p.node("protocols",80,370,380,125,"protocols",layer="L3")
    p.node("services",80,620,380,125,"services",layer="L3")
    p.node("drivers",80,875,380,125,"drivers/pl",layer="L3")
    p.node("ports",1330,370,390,140,"ports",layer="L1")
    p.artifact("shared",660,850,470,160,"shared, hardware","Интерфейсы и контракты")
    p.artifact("os",1330,640,390,130,"ОС и BSP","Системные библиотеки")
    p.path("apps","protocols",((760,225),(760,300),(270,300),(270,370)),"depends")
    p.path("apps","ports",((1030,225),(1030,300),(1525,300),(1525,370)),"depends")
    p.edge("protocols","services","depends",start="S",end="N")
    p.edge("services","drivers","depends",start="S",end="N")
    p.edge("drivers","shared","depends",start="E",end="W",b_offset=7.5)
    p.edge("ports","os","depends",start="S",end="N")
    p.path("ports","shared",((1330,440),(1200,440),(1200,930),(1130,930)),"depends")
    p.legend_note="Стрелка — зависимость исходников. Лист — общий контракт. Цвет — основная роль каталога."
    p.save()


def artifacts():
    p=Figure("build/artifacts.svg","Результаты сборки")
    p.artifact("rtl",70,110,430,135,"RTL, XDC, Tcl")
    p.tool("viv",680,110,380,135,"Vivado")
    p.artifact("export",1240,110,480,145,"design.xsa, design.bit","Один аппаратный экспорт")
    p.artifact("code",70,470,430,145,"Исходники BVSTK","Контракт платы и configs")
    p.tool("vitis",680,470,380,145,"Vitis")
    p.artifact("elf",1240,470,480,145,"FreeRTOS ELF","Приложение")
    p.tool("qcc",680,820,380,125,"qcc")
    p.tool("mkifs",680,1120,380,125,"mkifs")
    p.artifact("bsp",70,1110,430,145,"BSP и .build","Состав образа ОС")
    p.artifact("ifs",1240,1110,480,145,"IFS Нейтрино")
    p.edge("rtl","viv","build",start="E",end="W")
    p.edge("viv","export","build",start="E",end="W",b_offset=-5)
    p.edge("code","vitis","build",start="E",end="W")
    p.edge("vitis","elf","build",start="E",end="W")
    p.path("export","vitis",((1480,255),(1480,355),(870,355),(870,470)),"build","XSA",label_xy=(1160,333))
    p.path("code","qcc",((285,615),(285,882.5),(680,882.5)),"build")
    p.edge("qcc","mkifs","build",start="S",end="N",label="Программы Нейтрино",label_xy=(1020,1037))
    p.edge("bsp","mkifs","build",start="E",end="W")
    p.edge("mkifs","ifs","build",start="E",end="W")
    p.legend_note="Лист — файл или набор исходников. Скруглённый блок — инструмент сборки."
    p.save()


def boot_modes():
    p=Figure("build/boot-modes.svg","JTAG и автономная загрузка")
    p.artifact("jtag-input",60,140,500,200,"design.bit, ps7_init.tcl","FreeRTOS ELF либо IFS")
    p.tool("xsct",720,160,370,145,"XSCT")
    p.tool("memory",1290,160,440,145,"Загрузка через JTAG")
    p.artifact("boot-input",60,600,500,200,"FSBL, design.bit","FreeRTOS ELF")
    p.tool("bootgen",720,620,370,145,"Bootgen")
    p.artifact("boot",1290,605,440,175,"BOOT.bin","Автономный запуск из QSPI")
    p.edge("jtag-input","xsct","build",start="E",end="W",a_offset=-7.5)
    p.edge("xsct","memory","build",start="E",end="W")
    p.edge("boot-input","bootgen","build",start="E",end="W",a_offset=-7.5)
    p.edge("bootgen","boot","build",start="E",end="W")
    p.legend_note="Лист — входные файлы или образ. FSBL собирается из платформы того же XSA."
    p.save()


def network_change():
    p=Figure("operations/network-change.svg","Применение нового адреса",
             ["src/apps/freertos/services/http/http_fs_routes.c"],width=1800)
    p.tool("client",70,100,370,110,"Клиент")
    p.node("http",675,100,430,120,"HTTP",layer="L4")
    p.node("netif",1285,100,440,120,"netif",layer="L3")
    for x in (255,890,1505):
        el(p.under,"path",d=f"M{x},250 V1120",fill="none",stroke="#b6c0c8",stroke_width=2,stroke_dasharray="6 7")
    p.path("client","http",((255,310),(890,310)),"data","PUT: apply=true",label_xy=(565,290))
    p.artifact("save",680,390,425,125,"network.json","Попытка сохранения")
    p.path("http","save",((890,310),(890,390)),"data")
    p.path("http","netif",((890,615),(1505,615)),"call","Применить адрес",label_xy=(1197,592))
    p.text(p.notes,1560,695,".10 → .20",24,bold=True,color="#287f73")
    p.path("http","client",((890,780),(255,780)),"data","Ответ может потеряться",label_xy=(565,753))
    p.path("client","http",((255,985),(890,985)),"data","GET по новому адресу",label_xy=(565,959))
    p.path("http","client",((890,1110),(255,1110)),"data","Действующие параметры",label_xy=(565,1087))
    p.max_y=1140
    p.legend_note="Время идёт сверху вниз. Применение выполняется и при ошибке сохранения."
    p.save()


def main():
    from dcp2_introduction import generate as dcp2_introduction
    from dcp2_reference import generate as dcp2_reference
    from dcp2_examples import generate as dcp2_examples

    drawing.OUT=OUT
    figures = (startup,ps_pl,irq,configuration,package,volumes,network,
               requests,dcp2,observations,history,dependencies,artifacts,boot_modes,network_change)
    for generate in figures:
        generate()
    count = len(figures) + dcp2_introduction() + dcp2_reference() + dcp2_examples()
    print(f"Generated {count} documentation figures")


if __name__ == "__main__":
    main()
