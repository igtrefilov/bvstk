proc safe_stop {} {
    if {[catch {stop} err]} {
        if {![string match "*Already stopped*" $err]} {
            error $err
        }
    }
}

proc required_env {name} {
    if {![info exists ::env($name)] || $::env($name) eq ""} {
        error "Required environment variable is not set: $name"
    }
    return [file normalize $::env($name)]
}

set IFS_FILE [required_env IFS_FILE]
set PS7_INIT_TCL [required_env PS7_INIT_TCL]
set BITSTREAM_FILE [required_env BITSTREAM_FILE]

foreach path [list $IFS_FILE $PS7_INIT_TCL $BITSTREAM_FILE] {
    if {![file exists $path]} {
        error "Required JTAG input not found: $path"
    }
}

connect

targets -set -nocase -filter {name =~ "APU"}
rst -system
after 1000
safe_stop

puts "Programming PL: $BITSTREAM_FILE"
fpga -f $BITSTREAM_FILE

puts "Initializing PS7/DDR: $PS7_INIT_TCL"
source $PS7_INIT_TCL
ps7_init
ps7_post_config

# AX7020 hardware export uses a 100 MHz UART1 reference clock.  The inherited
# Zynq7000 startup expects 50 MHz, therefore keep the proven divisor adjustment.
mwr -force 0xF8000154 0x00001402

targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
safe_stop
puts "Downloading Neutrino IFS: $IFS_FILE"
dow -data $IFS_FILE 0x00100000
rwr pc 0x00100000

targets -set -nocase -filter {name =~ "*Cortex-A9*#1*"}
con
after 100

targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
con
after 7000
puts "Neutrino JTAG start completed"
