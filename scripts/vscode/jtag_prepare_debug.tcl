#
# jtag_prepare_debug.tcl — Zynq-7000: program PL + init PS7 + halt CPU
#
# Used as a pre-step before attaching GDB from VSCode.
#

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

set SCRIPT_DIR [file dirname [file normalize [info script]]]

# Bitstream selection:
# - if env(BITSTREAM_FILE) is set, use it
# - else use the exported platform bitstream (default Vitis export location)
set BITSTREAM_DEFAULT [file join $SCRIPT_DIR .. .. vitis_ws plat_bvstk export plat_bvstk hw Burevestnik_top.bit]
if {[info exists env(BITSTREAM_FILE)] && $env(BITSTREAM_FILE) ne ""} {
    set BITSTREAM_FILE $env(BITSTREAM_FILE)
} else {
    set BITSTREAM_FILE $BITSTREAM_DEFAULT
}

set PS7_INIT_TCL [file join $SCRIPT_DIR .. .. vitis_ws plat_bvstk export plat_bvstk hw ps7_init.tcl]

if {![file exists $BITSTREAM_FILE]} {
    error "Bitstream not found at $BITSTREAM_FILE. Build hardware platform first or set BITSTREAM_FILE."
}
if {![file exists $PS7_INIT_TCL]} {
    error "PS7 init script not found at $PS7_INIT_TCL"
}

puts "Connecting to hw_server..."
connect

# Select APU group (both cores) and reset/halt before touching PS registers.
targets -set -nocase -filter {name =~ "APU"}
reset_and_halt

puts "Programming programmable logic with $BITSTREAM_FILE"
fpga -f "$BITSTREAM_FILE"

puts "Sourcing PS7 initialization script: $PS7_INIT_TCL"
source $PS7_INIT_TCL

puts "Running PS7 initialization..."
ps7_init
ps7_post_config

# Halt core0 and leave it stopped for GDB attach.
targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
safe_stop

puts "JTAG prepare done (core0 halted). Now attach GDB from VSCode."
