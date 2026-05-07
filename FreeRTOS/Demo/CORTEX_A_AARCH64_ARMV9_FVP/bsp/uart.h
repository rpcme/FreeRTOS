#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_crlf(void);
void uart_puts(const char *s);
void uart_puthex(uint64_t val);
char uart_getc(void);
void uart_flush_rx(void);
void uart_hexdump(const void *addr, uint32_t len);

#endif /* UART_H */
