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

# The generated AX7020 PS7 init already configures UART1's 100 MHz reference
# clock (UART_CLK_CTRL = 0x00000A02).  Keep that value so 115200 baud remains
# correct for the BSP configuration.

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
