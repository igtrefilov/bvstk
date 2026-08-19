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

set PATCH_FFCONF_SCRIPT [file join $REPO_ROOT tools codegen patch_ffconf_lfn.py]
set GEN_DEFAULT_CONFIGS_SCRIPT [file join $REPO_ROOT tools codegen gen_default_configs.py]
set DEFAULT_CONFIGS_HDR [file join $REPO_ROOT src apps freertos config default_configs.h]
set SSH_CONFIG_SCRIPT [file join $REPO_ROOT scripts ssh generate_config.py]
set SSH_GENERATED_HDR [file join $REPO_ROOT src apps freertos services ssh bvstk_ssh_generated.h]

set ssh_enabled 0
if {[info exists ::env(BVSTK_SSH_ENABLE)] && $::env(BVSTK_SSH_ENABLE) == 1} {
    set ssh_enabled 1
}

proc validate_ssh_dependencies {wolfssl_root wolfssh_root} {
    if {![file exists [file join $wolfssl_root include wolfssl options.h]] ||
        ![file exists [file join $wolfssl_root lib libwolfssl.a]]} {
        error "wolfSSL root must contain include/wolfssl/options.h and lib/libwolfssl.a"
    }
    if {[file exists [file join $wolfssh_root lib libwolfssh.a]]} {
        set wolfssh_lib_dir [file join $wolfssh_root lib]
    } elseif {[file exists [file join $wolfssh_root src .libs libwolfssh.a]]} {
        set wolfssh_lib_dir [file join $wolfssh_root src .libs]
    } else {
        error "wolfSSH root must contain lib/libwolfssh.a or src/.libs/libwolfssh.a"
    }
    foreach header {ssh.h wolfscp.h wolfsftp.h} {
        if {![file exists [file join $wolfssh_root wolfssh $header]]} {
            error "wolfSSH root must contain wolfssh/$header"
        }
    }
    set nm_tool [auto_execok nm]
    if {$nm_tool == ""} {
        error "nm is required to verify the SSH library"
    }
    if {[catch {exec $nm_tool [file join $wolfssh_lib_dir libwolfssh.a]} nm_output]} {
        error "unable to inspect libwolfssh.a: $nm_output"
    }
    foreach symbol {wolfSSH_SetScpRecv wolfSSH_SetScpSend wolfSSH_SFTP_read wolfSSH_SFTP_SetDefaultPath} {
        if {[string first $symbol $nm_output] < 0} {
            error "libwolfssh.a does not contain $symbol; rebuild bundled wolfSSH with SCP/SFTP support"
        }
    }
    return $wolfssh_lib_dir
}

if {$ssh_enabled} {
    if {![info exists ::env(BVSTK_SSH_PASSWORD)] || $::env(BVSTK_SSH_PASSWORD) == ""} {
        error "BVSTK_SSH_PASSWORD must be set when BVSTK_SSH_ENABLE=1"
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

# Build a target-specific source view. Vitis 2021.2 recursively discovers
# files under the application source directory, so linking the repository's
# complete src/ tree would accidentally compile sources for other targets.
proc link_source_tree {source destination} {
    file mkdir $destination
    foreach entry [glob -nocomplain -directory $source *] {
        set target [file join $destination [file tail $entry]]
        if {[file isdirectory $entry]} {
            link_source_tree $entry $target
        } else {
            file link -symbolic $target $entry
        }
    }
}

setws $WS
gen_default_configs $GEN_DEFAULT_CONFIGS_SCRIPT $REPO_ROOT $DEFAULT_CONFIGS_HDR
platform create -name $PLAT_NAME -hw $XSA -proc $PROC -os $OS_RTOS -out $WS
platform active $PLAT_NAME

# Increase FreeRTOS heap for all tasks
if {$ssh_enabled} {
    # wolfSSH SCP allocates an additional per-session transfer state and
    # buffer; leave enough FreeRTOS heap for the existing services as well.
    catch {bsp config total_heap_size 1048576}
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
# Enable FatFs (xilffs) for PS SD, PL SD and QSPI volumes.
if {[catch {bsp setlib -name xilffs} msg]} {
    puts "xilffs library not found: $msg"
} else {
    # FatFs must expose 0:/ (PS SD), 1:/ (QSPI) and 2:/ (PL SD).
    if {[catch {bsp config num_logical_vol 3} volume_msg]} {
        error "Unable to configure three FatFs logical volumes: $volume_msg"
    }
    # diskio.c is compiled as part of the generated xilffs BSP library, so
    # give that makefile the repository source include root as well.
    set FS_SOURCE_INCLUDE [file normalize [file join $REPO_ROOT src]]
    if {[catch {bsp config -append extra_compiler_flags "-I$FS_SOURCE_INCLUDE"} include_msg]} {
        error "Unable to add the BVSTK source include path to the BSP: $include_msg"
    }
}
# Mount SD card in interrupt-driven mode; fall back silently if option is absent
catch {bsp config xilffs_polled_mode false}
catch {bsp config xilffs_fs_interface SD}
bsp regenerate
ensure_ffconf_lfn $PATCH_FFCONF_SCRIPT $WS

if {[file exists [file join $WS plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp/ps7_cortexa9_0/libsrc]]} {
    # Replace the generated diskio implementation with our shared version.
    # The xilffs directory is versioned and depends on the installed Vitis
    # release (2021.2 generates xilffs_v4_6, not xilffs_v5_3).
    set CUSTOM_DISKIO [file join $REPO_ROOT src ports freertos-xilinx fs-fatfs diskio.c]
    set LIBSRC [file join $WS plat_bvstk ps7_cortexa9_0 freertos10_xilinx_domain bsp ps7_cortexa9_0 libsrc]
    set DISKIO_TARGETS [glob -nocomplain -directory $LIBSRC xilffs_v*/src/diskio.c]
    if {[llength $DISKIO_TARGETS] == 0} {
        error "Generated xilffs diskio.c was not found under $LIBSRC"
    }
    foreach target $DISKIO_TARGETS {
        file copy -force $CUSTOM_DISKIO $target
    }
}

if {$ssh_enabled} {
    set has_external_ssl 0
    set has_external_ssh 0
    if {[info exists ::env(BVSTK_WOLFSSL_ROOT)] &&
        $::env(BVSTK_WOLFSSL_ROOT) != ""} {
        set has_external_ssl 1
    }
    if {[info exists ::env(BVSTK_WOLFSSH_ROOT)] &&
        $::env(BVSTK_WOLFSSH_ROOT) != ""} {
        set has_external_ssh 1
    }
    if {$has_external_ssl != $has_external_ssh} {
        error "BVSTK_WOLFSSL_ROOT and BVSTK_WOLFSSH_ROOT must be set together"
    }

    if {$has_external_ssl} {
        set WOLFSSL_ROOT [file normalize $::env(BVSTK_WOLFSSL_ROOT)]
        set WOLFSSH_ROOT [file normalize $::env(BVSTK_WOLFSSH_ROOT)]
        puts "Using externally supplied wolfSSL/wolfSSH dependencies"
    } else {
        set SSH_DEPS_SCRIPT [file join $REPO_ROOT scripts vitis build_ssh_deps.sh]
        set BSP_INCLUDE [file join $WS plat_bvstk export plat_bvstk sw \
                         plat_bvstk freertos10_xilinx_domain bspinclude include]
        if {![file isdirectory $BSP_INCLUDE]} {
            error "Generated BSP include directory not found at $BSP_INCLUDE"
        }
        set FATFS_INCLUDE [file join $WS plat_bvstk ps7_cortexa9_0 \
                           freertos10_xilinx_domain bsp ps7_cortexa9_0 include]
        if {![file exists [file join $FATFS_INCLUDE ff.h]]} {
            error "Generated FatFs include directory not found at $FATFS_INCLUDE"
        }
        set XILINX_INCLUDE [file join $WS plat_bvstk ps7_cortexa9_0 \
                            freertos10_xilinx_domain bsp ps7_cortexa9_0 \
                            libsrc standalone_v7_6 src]
        if {![file exists [file join $XILINX_INCLUDE xil_types.h]]} {
            error "Generated Xilinx include directory not found at $XILINX_INCLUDE"
        }
        set FREERTOS_INCLUDE [file join $WS plat_bvstk ps7_cortexa9_0 \
                              freertos10_xilinx_domain bsp ps7_cortexa9_0 \
                              libsrc freertos10_xilinx_v1_10 src]
        if {![file exists [file join $FREERTOS_INCLUDE FreeRTOS.h]]} {
            error "Generated FreeRTOS include directory not found at $FREERTOS_INCLUDE"
        }
        set BSP_LIBSRC [file join $WS plat_bvstk ps7_cortexa9_0 \
                        freertos10_xilinx_domain bsp ps7_cortexa9_0 libsrc]
        set LWIP_SOCKET_HEADERS [glob -nocomplain -type f -directory $BSP_LIBSRC \
                                 */src/*/src/include/lwip/sockets.h]
        if {[llength $LWIP_SOCKET_HEADERS] == 0} {
            error "Generated lwIP socket headers not found below $BSP_LIBSRC"
        }
        set LWIP_INCLUDE [file dirname [file dirname [lindex $LWIP_SOCKET_HEADERS 0]]]
        set LWIP_CONTRIB_CANDIDATES [glob -nocomplain -type d -directory $BSP_LIBSRC \
                                     */src/contrib/ports/xilinx/include]
        if {[llength $LWIP_CONTRIB_CANDIDATES] == 0} {
            error "Generated Xilinx lwIP headers not found below $BSP_LIBSRC"
        }
        set LWIP_CONTRIB_INCLUDE [lindex $LWIP_CONTRIB_CANDIDATES 0]
        set SSH_DEPS_OUTPUT [file join $REPO_ROOT build ssh-deps]
        puts "Building bundled wolfSSL/wolfSSH dependencies..."
        if {[catch {exec bash -- $SSH_DEPS_SCRIPT \
                    --bsp-include $BSP_INCLUDE --fatfs-include $FATFS_INCLUDE \
                    --xilinx-include $XILINX_INCLUDE \
                    --freertos-include $FREERTOS_INCLUDE \
                    --lwip-include $LWIP_INCLUDE \
                    --lwip-contrib-include $LWIP_CONTRIB_INCLUDE \
                    --output $SSH_DEPS_OUTPUT 2>@1} err]} {
            error "Bundled SSH dependency build failed: $err"
        }
        set WOLFSSL_ROOT [file join $SSH_DEPS_OUTPUT install wolfssl]
        set WOLFSSH_ROOT [file join $SSH_DEPS_OUTPUT install wolfssh]
    }
    set WOLFSSH_LIB_DIR [validate_ssh_dependencies $WOLFSSL_ROOT $WOLFSSH_ROOT]
}

app create -name $APP_NAME -platform $PLAT_NAME -template "Empty Application(C)"

set SRC_REAL [file normalize [file join $REPO_ROOT src]]
app config -name $APP_NAME -add include-path $SRC_REAL

if {$ssh_enabled} {
    app config -name $APP_NAME -add include-path [file join $WOLFSSL_ROOT include]
    app config -name $APP_NAME -add include-path $WOLFSSH_ROOT
    app config -name $APP_NAME -add compiler-misc "-DBVSTK_SSH_ENABLE -DWOLFSSH_SCP -DWOLFSSH_SCP_USER_CALLBACKS -DWOLFSSH_SFTP -DWOLFSSH_FATFS -DWOLFSSH_BVSTK_FATFS -DWOLFSSH_MAX_SFTP_RW=32768 -DWOLFSSH_MAX_SFTP_RECV=4096 -DWOLFSSH_TERM -DNO_TERMIOS -DWOLFSSL_LWIP -DNO_FILESYSTEM -DNO_WOLFSSL_DIR -DWC_RNG_SEED_CB"
    app config -name $APP_NAME -add library-search-path [file join $WOLFSSL_ROOT lib]
    app config -name $APP_NAME -add library-search-path $WOLFSSH_LIB_DIR
    app config -name $APP_NAME -add libraries wolfssh
    app config -name $APP_NAME -add libraries wolfssl
    app config -name $APP_NAME -add libraries m
}

set app_src   [file join $WS $APP_NAME src]
file delete -force $app_src
file mkdir $app_src

set FREERTOS_SOURCE_ROOTS [list \
    [file join apps freertos] \
    drivers \
    hardware \
    [file join ports freertos-xilinx board] \
    [file join ports freertos-xilinx fs-fatfs] \
    [file join ports freertos-xilinx os] \
    [file join ports freertos-xilinx storage] \
    protocols \
    services \
    shared \
    [file join vendor lwip]]

puts "Creating explicit FreeRTOS source view at $app_src"
foreach relative_root $FREERTOS_SOURCE_ROOTS {
    set source [file join $SRC_REAL $relative_root]
    if {![file isdirectory $source]} {
        error "FreeRTOS source root not found: $source"
    }
    link_source_tree $source [file join $app_src $relative_root]
}

# Keep the Vitis-generated make flow compatible with its conventional root
# linker-script locations while the canonical files live in the target port.
foreach linker_file {lscript.ld Xilinx.spec} {
    set source [file join $SRC_REAL ports freertos-xilinx linker $linker_file]
    file link -symbolic [file join $app_src $linker_file] $source
}

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
