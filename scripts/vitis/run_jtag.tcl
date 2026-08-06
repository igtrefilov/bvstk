#
# run_jtag.tcl — Zynq-7000: program PL + init PS7 + load ELF + run
#

# --- Helpers ---------------------------------------------------------

proc safe_stop {} {
    if {[catch {stop} err]} {
        if {[string match "*Already stopped*" $err]} {
            puts "CPU already stopped, continuing..."
        } else {
            error $err
        }
    }
}

proc reset_and_halt {} {
    puts "Resetting system and halting CPU..."
    if {[catch {rst -system} err]} {
        error "rst -system failed: $err"
    }
    after 1000
    safe_stop
}

# --- Paths -----------------------------------------------------------

set SCRIPT_DIR [file dirname [file normalize [info script]]]
set REPO_ROOT [file normalize [file join $SCRIPT_DIR .. ..]]

if {[info exists env(ELF_FILE)] && $env(ELF_FILE) ne ""} {
    set ELF_FILE [file normalize $env(ELF_FILE)]
} else {
    set ELF_FILE [file join $REPO_ROOT vitis_ws app_bvstk Debug app_bvstk.elf]
}

set BITSTREAM_DEFAULT_1 [file join $REPO_ROOT artifacts fpga design.bit]
set BITSTREAM_DEFAULT_2 [file join $REPO_ROOT .. bvstk_hw tmp design.bit]
set BITSTREAM_DEFAULT_3 [file join $REPO_ROOT vitis_ws plat_bvstk export plat_bvstk hw Burevestnik_top.bit]

if {[info exists env(BITSTREAM_FILE)] && $env(BITSTREAM_FILE) ne ""} {
    set BITSTREAM_FILE $env(BITSTREAM_FILE)
} elseif {[file exists $BITSTREAM_DEFAULT_1]} {
    set BITSTREAM_FILE $BITSTREAM_DEFAULT_1
} elseif {[file exists $BITSTREAM_DEFAULT_2]} {
    set BITSTREAM_FILE $BITSTREAM_DEFAULT_2
} else {
    set BITSTREAM_FILE $BITSTREAM_DEFAULT_3
}

if {[info exists env(PS7_INIT_TCL)] && $env(PS7_INIT_TCL) ne ""} {
    set PS7_INIT_TCL [file normalize $env(PS7_INIT_TCL)]
} else {
    set PS7_INIT_CANDIDATES [list \
        [file join $REPO_ROOT vitis_ws plat_bvstk export plat_bvstk hw ps7_init.tcl] \
        [file join $REPO_ROOT vitis_ws plat_bvstk hw ps7_init.tcl] \
        [file join $REPO_ROOT vitis_ws app_bvstk _ide psinit ps7_init.tcl]]
    set PS7_INIT_TCL ""
    foreach candidate $PS7_INIT_CANDIDATES {
        if {[file exists $candidate]} {
            set PS7_INIT_TCL [file normalize $candidate]
            break
        }
    }
    if {$PS7_INIT_TCL eq ""} {
        set PS7_INIT_TCL [lindex $PS7_INIT_CANDIDATES 0]
    }
}

# --- Sanity checks ---------------------------------------------------

if {![file exists $ELF_FILE]} {
    error "ELF file not found at $ELF_FILE. Please build the project first."
}
if {![file exists $BITSTREAM_FILE]} {
    error "Bitstream not found at $BITSTREAM_FILE. Please build the hardware platform first or set BITSTREAM_FILE."
}
if {![file exists $PS7_INIT_TCL]} {
    error "PS7 init script not found at $PS7_INIT_TCL"
}

# --- Connect ---------------------------------------------------------

puts "Connecting to hw_server..."
connect

# --- Reset/halt before touching PS registers -------------------------

# Select APU group (both cores)
targets -set -nocase -filter {name =~ "APU"}
reset_and_halt

# --- Program PL ------------------------------------------------------

puts "Programming programmable logic with $BITSTREAM_FILE"
fpga -f "$BITSTREAM_FILE"

# --- PS7 init --------------------------------------------------------

puts "Sourcing PS7 initialization script: $PS7_INIT_TCL"
source $PS7_INIT_TCL

puts "Running PS7 initialization..."
ps7_init
ps7_post_config

# AX7020 hardware export uses a 100 MHz UART1 reference clock.  The inherited
# Zynq7000 startup expects 50 MHz, therefore keep the proven divisor adjustment.
mwr -force 0xF8000154 0x00001402

# --- Load ELF on core0 and run --------------------------------------

targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
safe_stop

puts "Downloading ELF: $ELF_FILE"
dow "$ELF_FILE"

puts "Starting application (core0)..."
con

puts "Done."
