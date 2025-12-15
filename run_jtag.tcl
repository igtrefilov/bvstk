#
# This script connects to a Zynq target, downloads and runs the application.
#

# Path to the ELF file to be downloaded
set SCRIPT_DIR [file dirname [file normalize [info script]]]
set ELF_FILE [file join $SCRIPT_DIR vitis_ws app_bvstk Debug app_bvstk.elf]

if {![file exists $ELF_FILE]} {
    error "ELF file not found at $ELF_FILE. Please build the project first."
}

# Connect to the default hardware server
connect

# Select the Cortex-A9 Core 0 target
targets -set -nocase -filter {name =~ "*ARM*A9*#0*"}

# Reset the system
rst -system

# Source the PS7 initialization script and run initialization procedures
set PS7_INIT_TCL [file join $SCRIPT_DIR vitis_ws plat_bvstk export plat_bvstk hw ps7_init.tcl]
if {![file exists $PS7_INIT_TCL]} {
    error "PS7 init script not found at $PS7_INIT_TCL"
}
puts "Sourcing PS7 initialization script..."
source $PS7_INIT_TCL
puts "Running PS7 initialization..."
ps7_init
ps7_post_config

# Download the application ELF
puts "Downloading $ELF_FILE to target"
dow $ELF_FILE

# Continue execution
puts "Starting application"
con

puts "Application is running. Detaching from the target."
disconnect
