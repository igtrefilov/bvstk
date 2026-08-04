#
# One-stop XSCT build script.
# Creates a fresh Vitis workspace, generates the platform (FreeRTOS + lwIP),
# links the firmware sources from ./src, and builds the application ELF.
#
# You can override XSA and CLEAN via environment variables:
#   env XSA=/path/to/design.xsa CLEAN=0 xsct scripts/vitis/build.tcl
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

# Resolve repo root from script location.
set SCRIPT_DIR [file dirname [file normalize [info script]]]
set REPO_ROOT [file normalize [file join $SCRIPT_DIR .. ..]]

# Workspace / project names
set WS        [file normalize [file join $REPO_ROOT vitis_ws]]
set PLAT_NAME plat_bvstk
set APP_NAME  app_bvstk

# Tooling / design inputs
if {[info exists ::env(XSA)]} {
    set XSA [file normalize $::env(XSA)]
} else {
    set XSA_CANDIDATE_1 [file normalize [file join $REPO_ROOT artifacts fpga design.xsa]]
    set XSA $XSA_CANDIDATE_1
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

set PATCH_FFCONF_SCRIPT [file join $REPO_ROOT src scripts patch_ffconf_lfn.py]
set GEN_DEFAULT_CONFIGS_SCRIPT [file join $REPO_ROOT src scripts gen_default_configs.py]
set DEFAULT_CONFIGS_HDR [file join $REPO_ROOT src config default_configs.h]
set SSH_CONFIG_SCRIPT [file join $REPO_ROOT scripts ssh generate_config.py]
set SSH_GENERATED_HDR [file join $REPO_ROOT src ssh bvstk_ssh_generated.h]

set ssh_enabled 0
if {[info exists ::env(BVSTK_SSH_ENABLE)] && $::env(BVSTK_SSH_ENABLE) == 1} {
    set ssh_enabled 1
}

proc required_env {name} {
    if {![info exists ::env($name)] || $::env($name) == ""} {
        error "$name must be set when BVSTK_SSH_ENABLE=1"
    }
    return [file normalize $::env($name)]
}

if {$ssh_enabled} {
    set WOLFSSL_ROOT [required_env BVSTK_WOLFSSL_ROOT]
    set WOLFSSH_ROOT [required_env BVSTK_WOLFSSH_ROOT]
    if {![info exists ::env(BVSTK_SSH_PASSWORD)] || $::env(BVSTK_SSH_PASSWORD) == ""} {
        error "BVSTK_SSH_PASSWORD must be set when BVSTK_SSH_ENABLE=1"
    }
    if {![file exists [file join $WOLFSSL_ROOT include wolfssl options.h]] ||
        ![file exists [file join $WOLFSSL_ROOT lib libwolfssl.a]]} {
        error "BVSTK_WOLFSSL_ROOT must contain include/wolfssl and lib/libwolfssl.a"
    }
    if {[file exists [file join $WOLFSSH_ROOT lib libwolfssh.a]]} {
        set WOLFSSH_LIB_DIR [file join $WOLFSSH_ROOT lib]
    } elseif {[file exists [file join $WOLFSSH_ROOT src .libs libwolfssh.a]]} {
        set WOLFSSH_LIB_DIR [file join $WOLFSSH_ROOT src .libs]
    } else {
        error "BVSTK_WOLFSSH_ROOT must contain lib/libwolfssh.a or src/.libs/libwolfssh.a"
    }
    if {![file exists [file join $WOLFSSH_ROOT wolfssh ssh.h]]} {
        error "BVSTK_WOLFSSH_ROOT must contain wolfssh/ssh.h"
    }
    puts "Generating build-local SSH credentials..."
    if {[catch {exec python3 -- $SSH_CONFIG_SCRIPT --output $SSH_GENERATED_HDR} err]} {
        error "SSH credential generation failed: $err"
    }
}

proc gen_default_configs {script repo_root out_hdr} {
    if {![file exists $script]} {
        error "gen_default_configs.py not found at $script"
    }
    puts "Generating embedded default configs..."
    if {[catch {exec python3 -- $script --repo-root $repo_root --out $out_hdr} err]} {
        error "gen_default_configs.py failed: $err"
    }
}

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
gen_default_configs $GEN_DEFAULT_CONFIGS_SCRIPT $REPO_ROOT $DEFAULT_CONFIGS_HDR
platform create -name $PLAT_NAME -hw $XSA -proc $PROC -os $OS_RTOS -out $WS
platform active $PLAT_NAME

# Increase FreeRTOS heap for all tasks
if {$ssh_enabled} {
    catch {bsp config total_heap_size 524288}
} else {
    catch {bsp config total_heap_size 131072}
}

# Attach lwIP (prefer 2.2.0, but fall back to 2.1.1 if needed)
set preferred_lwip_libs [list]
if {[info exists ::env(LWIP_LIB)]} {
    lappend preferred_lwip_libs $::env(LWIP_LIB)
} else {
    set preferred_lwip_libs [list lwip220 lwip211]
}

set lwip_lib ""
foreach candidate $preferred_lwip_libs {
    if {[catch {bsp setlib -name $candidate}]} {
        puts "lwIP candidate \"$candidate\" not available; trying next option."
    } else {
        set lwip_lib $candidate
        break
    }
}
if {$lwip_lib == ""} {
    error "None of the preferred lwIP libraries were found ([join $preferred_lwip_libs , ])."
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
    set CUSTOM_DISKIO [file join $REPO_ROOT src fs diskio.c]
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

if {$ssh_enabled} {
    app config -name $APP_NAME -add include-path [file join $WOLFSSL_ROOT include]
    app config -name $APP_NAME -add include-path $WOLFSSH_ROOT
    app config -name $APP_NAME -add compiler-misc "-DBVSTK_SSH_ENABLE -DWOLFSSH_TERM -DNO_TERMIOS -DWOLFSSL_LWIP -DNO_FILESYSTEM -DNO_WOLFSSL_DIR -DWC_RNG_SEED_CB"
    app config -name $APP_NAME -add library-search-path [file join $WOLFSSL_ROOT lib]
    app config -name $APP_NAME -add library-search-path $WOLFSSH_LIB_DIR
    app config -name $APP_NAME -add libraries wolfssh
    app config -name $APP_NAME -add libraries wolfssl
    app config -name $APP_NAME -add libraries m
}

set app_src   [file join $WS $APP_NAME src]
file delete -force $app_src

set SRC_REAL   [file normalize [file join $REPO_ROOT src]]

puts "Linking $app_src -> $SRC_REAL"

file link -symbolic $app_src $SRC_REAL

# Generate platform/BSP and build the application
platform generate
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS
app build -name $APP_NAME
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS

set ELF_PATH [file join $WS $APP_NAME Debug ${APP_NAME}.elf]
if {![file exists $ELF_PATH]} {
    error "Application build did not produce ELF: $ELF_PATH"
}

puts ""
puts "Build completed."
puts "Workspace : $WS"
puts "Platform  : $PLAT_NAME"
puts "Application ELF: $ELF_PATH"
