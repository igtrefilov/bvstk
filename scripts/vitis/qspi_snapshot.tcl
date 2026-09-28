# Read the complete AX7020 QSPI into a host file without programming Flash.
# Arguments: ps7_init.tcl qspi_read.elf output.bin [hw_server URL]
if {[llength $argv] < 3 || [llength $argv] > 4} {
    error "Usage: qspi_snapshot.tcl PS7_INIT HELPER_ELF OUTPUT_BIN ?URL?"
}
lassign $argv psinit helper output url
if {$url eq ""} { set url "tcp:127.0.0.1:3121" }
if {[file exists $output]} { error "Snapshot already exists: $output" }
connect -url $url
set cores [targets -filter {name =~ "*Cortex-A9*#0*"} -target-properties]
if {[llength $cores] != 1} { error "Expected exactly one Cortex-A9 #0 target" }
targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
set bootmode [mrd -value 0xF800025C]
if {[expr {$bootmode & 0xF}] != 0} {
    error "Select JTAG boot on AX7020 J13 and power-cycle before programming QSPI"
}
targets -set -nocase -filter {name =~ "APU"}
rst -system
after 1000
targets -set -nocase -filter {name =~ "*Cortex-A9*#0*"}
if {[catch {stop} err] && ![string match "*Already stopped*" $err]} { error $err }
source $psinit
ps7_init
# PL is not needed by the QSPI reader.
mwr 0x07000000 0
dow $helper
con
set complete 0
for {set n 0} {$n < 120} {incr n} {
    after 500
    set status [mrd -value 0x07000000]
    if {[expr {$status == 0x51444F4E}]} { set complete 1; break }
    if {[expr {$status == 0x51455252}]} {
        error "QSPI snapshot failed: [mrd 0x07000000 4]"
    }
}
if {!$complete} { error "QSPI snapshot timeout: [mrd 0x07000000 4]" }
stop
puts "QSPI JEDEC ID: [mrd -value 0x07000004]"
puts "Reading 32 MiB from DDR to $output"
mrd -bin -file $output 0x08000000 8388608
disconnect
puts "QSPI snapshot complete"
