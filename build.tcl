set WS        [file normalize "./vitis_ws"]
file mkdir $WS
set PLAT_NAME plat_bvstk
set APP_NAME  app_bvstk
set XSA       {/home/ilya/Zynq/xsa/alinx/Burevestnik_top.xsa}
set PROC      ps7_cortexa9_0
set OS_RTOS   freertos10_xilinx

setws $WS
platform create -name $PLAT_NAME -hw $XSA -proc $PROC -os $OS_RTOS -out $WS
platform active $PLAT_NAME

bsp setlib -name lwip211 -ver 1.6
bsp config api_mode SOCKET_API
bsp regenerate

app create -name $APP_NAME -platform $PLAT_NAME -template "Empty Application(C)"

# --- ЗДЕСЬ: заменяем внутренний src на симлинк на ВАШ репо src ---
set app_src   [file join $WS $APP_NAME src]
file delete -force $app_src

# Папка src в репозитории (рядом с build.tcl), считаем от пути скрипта:
set SCRIPT_DIR [file dirname [file normalize [info script]]]
set SRC_REAL   [file normalize [file join $SCRIPT_DIR src]]

# Для контроля можно вывести:
puts "Linking $app_src -> $SRC_REAL"

# Создаём симлинк (Linux)
file link -symbolic $app_src $SRC_REAL
