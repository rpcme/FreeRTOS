/*
 * FVP Base RevC AEMvA Platform Definitions
 * Memory map from Linux DTS: arch/arm64/boot/dts/arm/fvp-base-revc.dts
 */

#ifndef PLATFORM_FVP_H
#define PLATFORM_FVP_H

#include <stdint.h>

/* ---- UART (ARM PL011) ---- */
#define UART0_BASE  0x1c090000UL
#define UART1_BASE  0x1c0a0000UL
#define UART2_BASE  0x1c0b0000UL
#define UART3_BASE  0x1c0c0000UL

/* PL011 register offsets (same as Orion O6) */
#define PL011_DR        0x000
#define PL011_FR        0x018
#define PL011_IBRD      0x024
#define PL011_FBRD      0x028
#define PL011_LCR_H     0x02C
#define PL011_CR        0x030
#define PL011_IMSC      0x038
#define PL011_ICR       0x044

#define PL011_FR_TXFF   (1 << 5)
#define PL011_FR_RXFE   (1 << 4)
#define PL011_FR_BUSY   (1 << 3)

#define PL011_LCR_H_WLEN_8  (3 << 5)
#define PL011_LCR_H_FEN     (1 << 4)

#define PL011_CR_UARTEN  (1 << 0)
#define PL011_CR_TXE     (1 << 8)
#define PL011_CR_RXE     (1 << 9)

/* FVP PL011 clock: 14.7456 MHz (default) */
#define UART_CLK_HZ     14745600UL
#define UART_BAUD       115200

/* Default debug UART — UART0 on FVP */
#define DEBUG_UART_BASE  UART0_BASE

/* ---- GIC (GICv3, system register interface) ---- */
#define GICD_BASE       0x2f000000UL
#define GICR_BASE       0x2f100000UL
#define GICR_STRIDE     0x20000UL

/* ---- Generic Timer ---- */
/* FVP default: 100 MHz (read at runtime via CNTFRQ_EL0) */
#define TIMER_PPI       27  /* Virtual timer PPI INTID */

/* ---- Memory Map ---- */
#define DRAM_BASE       0x80000000UL
#define DRAM_SIZE       0x80000000UL  /* 2 GB (FVP default) */

/* ---- Helper macros ---- */
#define MMIO32(addr)    (*(volatile uint32_t *)(addr))

#endif /* PLATFORM_FVP_H */
