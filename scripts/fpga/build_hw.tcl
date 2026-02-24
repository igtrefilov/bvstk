# Build bitstream and XSA for the Burevestnik Zynq design.
# Defaults assume repository at ../hw_platform/fpga relative to this script.
# Usage:
#   vivado -mode batch -source build_hw.tcl -tclargs \
#     --fpga_dir <dir> --project_name <name> --jobs <n> --output_dir <dir>

# Resolve repository root containing Vivado project scripts (default: ../../../hw_platform/fpga)
set script_dir [file dirname [info script]]
set fpga_dir [file normalize "$script_dir/../../../hw_platform/fpga"]
set proj_name "Burevestnik_21"
set jobs 8
set output_dir [file normalize "$script_dir/../../artifacts/fpga"]

# Parse -tclargs
for {set i 0} {$i < $::argc} {incr i} {
  set opt [lindex $::argv $i]
  switch -- $opt {
    --fpga_dir { incr i; set fpga_dir [file normalize [lindex $::argv $i]] }
    --project_name { incr i; set proj_name [lindex $::argv $i] }
    --jobs { incr i; set jobs [lindex $::argv $i] }
    --output_dir { incr i; set output_dir [file normalize [lindex $::argv $i]] }
    default {}
  }
}

# Paths
set proj_path [file normalize "$fpga_dir/vivado_project/${proj_name}.xpr"]
if { ![file exists $proj_path] } {
  puts "ERROR: project '$proj_path' not found. Run Burevestnik_21.tcl first."
  exit 1
}

file mkdir $output_dir

open_project $proj_path
catch {reset_run synth_1}
catch {reset_run impl_1}
launch_runs impl_1 -to_step write_bitstream -jobs $jobs
wait_on_run impl_1

set impl_dir [get_property DIRECTORY [get_runs impl_1]]
# Try standard name first
set bit_src [file normalize "$impl_dir/${proj_name}.bit"]
if { ![file exists $bit_src] } {
  # fallback: pick the first .bit in impl directory
  set candidates [glob -nocomplain -types f [file join $impl_dir *.bit]]
  if { [llength $candidates] > 0 } {
    set bit_src [lindex $candidates 0]
  }
}
if { ![file exists $bit_src] } {
  puts "ERROR: bitstream not found in $impl_dir"
  exit 1
}
set bit_out [file normalize "$output_dir/design.bit"]
file copy -force $bit_src $bit_out
puts "Saved bitstream to $bit_out"

set xsa_out [file normalize "$output_dir/design.xsa"]
write_hw_platform -fixed -include_bit -force $xsa_out
puts "Saved XSA to $xsa_out"

exit
