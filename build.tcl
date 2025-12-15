#
# One-stop XSCT build script.
# Creates a fresh Vitis workspace, generates the platform (FreeRTOS + lwIP),
# links the firmware sources from ./src, and builds the application ELF.
#
# You can override XSA and CLEAN via environment variables:
#   env XSA=/path/to/design.xsa CLEAN=0 xsct build.tcl
#

# Helper to delete a path even if previous tools left odd permissions behind.
proc safe_delete {path} {
    if {![file exists $path]} {
        return
    }
    if {[catch {file delete -force $path} err]} {
        puts "XSCT failed to delete $path: $err"
        puts "Retrying with rm -rf ..."
        catch {exec chmod -R u+w -- $path}
        if {[catch {exec rm -rf -- $path} err2]} {
            error "Unable to remove $path: $err2"
        }
    }
}

# Workspace / project names
set WS        [file normalize "[file dirname [info script]]/vitis_ws"]
set PLAT_NAME plat_bvstk
set APP_NAME  app_bvstk

# Tooling / design inputs
if {[info exists ::env(XSA)]} {
    set XSA [file normalize $::env(XSA)]
} else {
    set XSA [file normalize "/home/ilya/Zynq/bvstk_hw/Burevestnik_top.xsa"]
}
set PROC      ps7_cortexa9_0
set OS_RTOS   freertos10_xilinx

# Clean previous workspace unless CLEAN=0
set do_clean 1
if {[info exists ::env(CLEAN)] && $::env(CLEAN) == 0} {
    set do_clean 0
}
if {$do_clean && [file exists $WS]} {
    puts "Removing previous workspace at $WS"
    safe_delete $WS
}
file mkdir $WS

set SCRIPT_DIR [file dirname [file normalize [info script]]]
set PATCH_FFCONF_SCRIPT [file join $SCRIPT_DIR src scripts patch_ffconf_lfn.py]

proc ensure_ffconf_lfn {script ws} {
    if {![file exists $script]} {
        return
    }
    puts "Ensuring FatFs LFN support via [file tail $script]"
    if {[catch {exec python3 -- $script $ws} err]} {
        error "patch_ffconf_lfn.py failed: $err"
    }
}

setws $WS
platform create -name $PLAT_NAME -hw $XSA -proc $PROC -os $OS_RTOS -out $WS
platform active $PLAT_NAME

# Increase FreeRTOS heap for all tasks
catch {bsp config total_heap_size 131072}

# Attach lwIP (prefer 2.1.1 if present, otherwise 2.2.0 from Vitis 2024.2)
set lwip_lib "lwip220"
if {[catch {bsp setlib -name $lwip_lib}]} {
    error "lwIP library \"$lwip_lib\" not found in current Vitis installation"
}
if {[catch {bsp config api_mode SOCKET_API}]} {
    puts "api_mode option not available for $lwip_lib, using defaults"
}
# Enable FatFs (xilffs) for SD-card access via PS SDIO0
# Enable FatFs (xilffs) for SD-card access via PS SDIO0
if {[catch {bsp setlib -name xilffs} msg]} {
    puts "xilffs library not found: $msg"
} else {
    # Replace the generated diskio implementation with our shared version.
    set CUSTOM_DISKIO [file join $SCRIPT_DIR src fs diskio.c]
    set TARGET_DISKIO [file join $WS plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp/ps7_cortexa9_0/libsrc/xilffs_v5_3/src/diskio.c]
    exec mkdir -p [file dirname $TARGET_DISKIO]
    file copy -force $CUSTOM_DISKIO $TARGET_DISKIO
}
# Mount SD card in interrupt-driven mode; fall back silently if option is absent
catch {bsp config xilffs_polled_mode false}
catch {bsp config xilffs_fs_interface SD}
bsp regenerate
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS

app create -name $APP_NAME -platform $PLAT_NAME -template "Empty Application(C)"

set app_src   [file join $WS $APP_NAME src]
file delete -force $app_src

set SRC_REAL   [file normalize [file join $SCRIPT_DIR src]]

puts "Linking $app_src -> $SRC_REAL"

file link -symbolic $app_src $SRC_REAL

# Generate platform/BSP and build the application
platform generate
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS
app build -name $APP_NAME
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS

puts ""
puts "Build completed."
puts "Workspace : $WS"
puts "Platform  : $PLAT_NAME"
puts "Application ELF: [file join $WS $APP_NAME Debug ${APP_NAME}.elf]"
