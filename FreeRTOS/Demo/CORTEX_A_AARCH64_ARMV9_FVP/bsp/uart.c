/*
 * PL011 UART driver — polling mode, no .rodata string references.
 * All string output built from immediates on the stack.
 */

#include "platform.h"
#include "uart.h"

void uart_init(void)
{
    /* Disable UART */
    MMIO32(DEBUG_UART_BASE + PL011_CR) = 0;
    /* Clear interrupts */
    MMIO32(DEBUG_UART_BASE + PL011_ICR) = 0x7FF;
    /* Compute baud rate divisors from UART_CLK_HZ and UART_BAUD */
    uint32_t divider = UART_CLK_HZ / (16 * UART_BAUD);
    uint32_t frac = ((UART_CLK_HZ % (16 * UART_BAUD)) * 64 + (16 * UART_BAUD) / 2) / (16 * UART_BAUD);
    MMIO32(DEBUG_UART_BASE + PL011_IBRD) = divider;
    MMIO32(DEBUG_UART_BASE + PL011_FBRD) = frac;
    /* 8N1, FIFO enable */
    MMIO32(DEBUG_UART_BASE + PL011_LCR_H) = PL011_LCR_H_WLEN_8 | PL011_LCR_H_FEN;
    /* Enable UART + TX + RX */
    MMIO32(DEBUG_UART_BASE + PL011_CR) = PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;
}

void uart_putc(char c)
{
    while (MMIO32(DEBUG_UART_BASE + PL011_FR) & PL011_FR_TXFF)
        ;
    MMIO32(DEBUG_UART_BASE + PL011_DR) = (uint32_t)c;
}

void uart_crlf(void)
{
    uart_putc('\r');
    uart_putc('\n');
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_puthex(uint64_t val)
{
    int i;
    uart_putc('0');
    uart_putc('x');
    for (i = 60; i >= 0; i -= 4) {
        unsigned n = (unsigned)((val >> i) & 0xf);
        uart_putc((char)(n < 10 ? '0' + n : 'a' + n - 10));
    }
}

char uart_getc(void)
{
    /* Wait while RX FIFO is empty */
    while (MMIO32(DEBUG_UART_BASE + PL011_FR) & PL011_FR_RXFE) {
#ifdef FVP_PLATFORM
        /* Brief spin to avoid hammering the Device register every cycle */
        for (volatile int i = 0; i < 10000; i++) ;
#endif
    }
    return (char)(MMIO32(DEBUG_UART_BASE + PL011_DR) & 0xFF);
}

void uart_hexdump(const void *addr, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) {
        if ((i & 0xF) == 0) {
            uart_puthex((uint64_t)(p + i));
            uart_putc(':');
            uart_putc(' ');
        }
        /* Print byte as 2 hex digits */
        unsigned hi = (p[i] >> 4) & 0xF;
        unsigned lo = p[i] & 0xF;
        uart_putc((char)(hi < 10 ? '0' + hi : 'a' + hi - 10));
        uart_putc((char)(lo < 10 ? '0' + lo : 'a' + lo - 10));
        uart_putc(' ');
        if ((i & 0xF) == 0xF || i == len - 1)
            uart_putc('\n');
    }
}

void uart_flush_rx(void)
{
    /* Drain any pending RX characters */
    while (!(MMIO32(DEBUG_UART_BASE + PL011_FR) & PL011_FR_RXFE))
        (void)MMIO32(DEBUG_UART_BASE + PL011_DR);
}
