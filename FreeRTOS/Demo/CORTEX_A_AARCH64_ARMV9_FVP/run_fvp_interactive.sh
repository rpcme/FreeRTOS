#!/bin/bash
# Run FreeRTOS on FVP — output to stdout
#
# For input: use telnet localhost 5000 from another terminal
# Output appears directly here (no telnet needed for viewing).
#
# Press Ctrl+C to stop.

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
FVP=~/toolchains/FVP_Base_RevC_AEMvA/bin/FVP_Base_RevC-2xAEMvA
ELF="$DIR/freertos_fvp.elf"

# Always clean rebuild
echo "Building (interactive)..."
rm -rf "$DIR/build/fvp" "$ELF"
make -C "$DIR" fvp FVP_EXTRA_DEFS= 2>&1 | tail -1

echo ""
echo "=== FVP Running ==="
echo "Output below. For input: telnet localhost 5000"
echo "Ctrl+C to stop."
echo "==================="
echo ""

$FVP \
    -C cluster0.cpu0.RVBAR=0x80000000 \
    -C cluster0.PA_SIZE=48 \
    -C cluster0.has_arm_v9-2=1 \
    -C cluster0.has_sve=1 \
    -C cluster0.sve.has_sve2=1 \
    -C cluster0.sve.veclen=2 \
    -C cluster0.memory_tagging_support_level=3 \
    -C cluster0.has_pointer_authentication=1 \
    -C cluster0.bti_support_level=1 \
    -C bp.secure_memory=false \
    -C bp.dram_metadata.is_enabled=1 \
    -C bp.refcounter.non_arch_start_at_default=1 \
    -C gic_distributor.wakeup-on-reset=1 \
    -C gic_distributor.ARE-fixed-to-one=1 \
    -C bp.pl011_uart0.out_file=- \
    -C bp.pl011_uart0.unbuffered_output=1 \
    -C bp.terminal_0.start_telnet=0 \
    -C bp.terminal_0.start_port=5000 \
    -C bp.terminal_1.start_telnet=0 \
    -C bp.terminal_2.start_telnet=0 \
    -C bp.terminal_3.start_telnet=0 \
    -C cluster0.cpu0.semihosting-cwd=$DIR \
    -C bp.vis.disable_visualisation=1 \
    -a "$ELF"
