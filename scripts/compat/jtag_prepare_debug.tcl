# Compatibility wrapper. Real script moved to scripts/vscode/.
set SCRIPT_DIR [file dirname [file normalize [info script]]]
source [file join $SCRIPT_DIR vscode jtag_prepare_debug.tcl]
