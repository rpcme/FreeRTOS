/*
 * MTE Demo — Buffer Overrun Detection (non-interactive)
 *
 * Tags a 32-byte buffer with MTE, writes within bounds (OK),
 * then writes 1 byte past the end. The adjacent granule has
 * tag 0 — mismatch triggers a synchronous data abort (DFSC=0x11).
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

extern void uart_puts(const char *s);
extern void uart_putc(char c);

void vMteDemoTask(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(300));

    uart_puts("\n[MTE] Memory Tagging demo\n");

    /* 64-byte buffer: tag first 32 bytes, leave rest as guard (tag 0) */
    volatile uint8_t buf[64] __attribute__((aligned(16)));

    /* Clear all tags to 0 */
    uint64_t raw = (uint64_t)buf;
    for (uint32_t i = 0; i < 64; i += 16)
        __asm volatile("stg %0, [%0]" :: "r"(raw + i));

    /* Tag first 32 bytes with a random non-zero tag */
    uint64_t tagged;
    __asm volatile("irg %0, %1" : "=r"(tagged) : "r"(raw));
    __asm volatile("stg %0, [%0]" :: "r"(tagged));
    __asm volatile("stg %0, [%0]" :: "r"(tagged + 16));

    uint8_t tag = (tagged >> 56) & 0xF;
    uart_puts("[MTE] Buffer tag=");
    uart_putc("0123456789ABCDEF"[tag]);
    uart_puts(", adjacent=0\n");

    /* Write 32 bytes in-bounds — tag matches, no fault */
    volatile uint8_t *p = (volatile uint8_t *)tagged;
    uart_puts("[MTE] Writing 32 bytes in-bounds... ");
    for (int i = 0; i < 32; i++)
        p[i] = (uint8_t)i;
    uart_puts("OK\n");

    /* Write 1 byte out-of-bounds — tag mismatch → fault */
    uart_puts("[MTE] Writing 1 byte out-of-bounds...\n");
    p[32] = 0xFF;

    /* Should not reach here */
    uart_puts("[MTE] ERROR: no fault!\n");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
