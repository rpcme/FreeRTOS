#!/bin/bash
# Run FreeRTOS on FVP — non-interactive test with wall-clock timeout
# Usage: ./run_fvp.sh [timeout_seconds]
# Default: 10 seconds

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
FVP=~/toolchains/FVP_Base_RevC_AEMvA/bin/FVP_Base_RevC-2xAEMvA
ELF="$DIR/freertos_fvp.elf"
UART_OUT=/tmp/fvp_uart0.txt
TIMEOUT=${1:-10}

# Build if needed
if [ ! -f "$ELF" ] || find "$DIR/src" "$DIR/include" "$DIR/linker" -newer "$ELF" 2>/dev/null | grep -q .; then
    echo "Building (non-interactive)..."
    rm -rf "$DIR/build/fvp"
    make -C "$DIR" fvp FVP_EXTRA_DEFS=-DFVP_NONINTERACTIVE
fi

rm -f "$UART_OUT"

echo "Running FVP (${TIMEOUT}s wall-clock timeout)..."

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
    -C bp.pl011_uart0.out_file="$UART_OUT" \
    -C bp.pl011_uart0.shutdown_on_eot=1 \
    -C cluster0.cpu0.semihosting-cwd=$DIR \
    -C bp.vis.disable_visualisation=1 \
    -C bp.terminal_0.start_telnet=0 \
    -C bp.terminal_1.start_telnet=0 \
    -C bp.terminal_2.start_telnet=0 \
    -C bp.terminal_3.start_telnet=0 \
    --stat \
    -T "$TIMEOUT" \
    -a "$ELF" 2>&1 | grep -E "MIPS|Stopping|stopping"

echo ""
echo "=== UART OUTPUT ==="
if [ -s "$UART_OUT" ]; then
    cat "$UART_OUT"
else
    echo "(empty)"
fi
echo ""
echo "=== END ==="
