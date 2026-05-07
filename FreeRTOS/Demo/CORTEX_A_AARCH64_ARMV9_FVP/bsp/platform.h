/*
 * Platform Definitions — selects board-specific header.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef FVP_PLATFORM
#include "platform_fvp.h"
#else
/*
 * CIX P1 (Sky1) Platform Definitions
 *
 * Register addresses extracted from Linux device tree:
 * https://github.com/cixtech/cix_opensource__linux/blob/cix_k6.6.89_2025q4/
 *   arch/arm64/boot/dts/cix/sky1.dtsi
 */

#include <stdint.h>

/* ---- UART (ARM PL011) ---- */
#define UART0_BASE  0x040b0000UL
#define UART1_BASE  0x040c0000UL
#define UART2_BASE  0x040d0000UL  /* Debug console on Orion O6 */
#define UART3_BASE  0x040e0000UL
#define UART4_BASE  0x06540000UL
#define UART5_BASE  0x05020000UL

/* PL011 register offsets */
#define PL011_DR        0x000  /* Data register */
#define PL011_FR        0x018  /* Flag register */
#define PL011_IBRD      0x024  /* Integer baud rate divisor */
#define PL011_FBRD      0x028  /* Fractional baud rate divisor */
#define PL011_LCR_H     0x02C  /* Line control */
#define PL011_CR        0x030  /* Control register */
#define PL011_IMSC      0x038  /* Interrupt mask set/clear */
#define PL011_ICR       0x044  /* Interrupt clear register */

/* PL011 flag register bits */
#define PL011_FR_TXFF   (1 << 5)  /* TX FIFO full */
#define PL011_FR_RXFE   (1 << 4)  /* RX FIFO empty */
#define PL011_FR_BUSY   (1 << 3)  /* UART busy */

/* PL011 line control bits */
#define PL011_LCR_H_WLEN_8  (3 << 5)  /* 8-bit word */
#define PL011_LCR_H_FEN     (1 << 4)  /* FIFO enable */

/* PL011 control register bits */
#define PL011_CR_UARTEN  (1 << 0)  /* UART enable */
#define PL011_CR_TXE     (1 << 8)  /* TX enable */
#define PL011_CR_RXE     (1 << 9)  /* RX enable */

/* UART clock: 100 MHz */
#define UART_CLK_HZ     100000000UL
#define UART_BAUD       115200

/* Default debug UART */
#define DEBUG_UART_BASE  UART2_BASE

/* ---- GIC-700 (GICv3) ---- */
#define GICD_BASE       0x0e010000UL  /* Distributor */
#define GICR_BASE       0x0e090000UL  /* Redistributor (CPU0) */
#define GICR_STRIDE     0x40000UL     /* Per-core redistributor spacing */

/* ---- Generic Timer ---- */
#define TIMER_FREQ_HZ   1000000000UL  /* 1 GHz (from DT clock-frequency) */
/* Virtual timer PPI = 11 (GIC INTID 27) — used at EL1 */
#define TIMER_PPI       27

/* ---- GPIO (Cadence r1p02) ---- */
#define FCH_GPIO0_BASE  0x04120000UL
#define FCH_GPIO1_BASE  0x04130000UL
#define FCH_GPIO2_BASE  0x04140000UL
#define FCH_GPIO3_BASE  0x04150000UL
#define S5_GPIO0_BASE   0x16004000UL
#define S5_GPIO1_BASE   0x16005000UL
#define S5_GPIO2_BASE   0x16006000UL

/* Cadence GPIO register offsets */
#define GPIO_SWPORTA_DR   0x00  /* Data register */
#define GPIO_SWPORTA_DDR  0x04  /* Data direction (1=output) */
#define GPIO_EXT_PORTA    0x50  /* External port (read pin state) */

/* ---- Memory Map ---- */
#define DRAM_BASE       0x80000000UL

/* ---- Helper macros ---- */
#define MMIO32(addr)    (*(volatile uint32_t *)(addr))

#endif /* !FVP_PLATFORM */
#endif /* PLATFORM_H */
