/*
 * PAC Demo — Pointer Authentication fault demonstration.
 *
 * Calls a function that corrupts its own saved LR on the stack,
 * then returns. AUTIASP detects the mismatch and raises a fault.
 *
 * Compiled with -mbranch-protection=standard, so every function
 * has PACIASP at entry and AUTIASP at return.
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

extern void uart_putc(char c);
extern void uart_puts(const char *s);

static volatile int pac_fault_caught = 0;

/* Called from the data abort handler when PAC auth fails */
void pac_fault_handler(void)
{
    pac_fault_caught = 1;
}

/*
 * This function corrupts its own saved LR (return address) on the stack.
 * On return, AUTIASP will fail because the PAC in the upper bits of LR
 * no longer matches the corrupted address.
 */
static void __attribute__((noinline, optimize("O0")))
corrupt_return_address(void)
{
    /* Get frame pointer (X29). The saved LR is at [X29 + 8]. */
    uint64_t *lr_on_stack;
    __asm volatile("mov %0, x29" : "=r"(lr_on_stack));
    lr_on_stack = (uint64_t *)((uint8_t *)lr_on_stack + 8);

    uart_puts("  LR before: ");
    uint64_t orig = *lr_on_stack;
    for (int i = 60; i >= 0; i -= 4)
        uart_putc("0123456789ABCDEF"[(orig >> i) & 0xF]);
    uart_putc('\n');

    /* Corrupt one bit in the address portion (not the PAC bits) */
    *lr_on_stack = orig ^ 0x4;

    uart_puts("  LR after:  ");
    uint64_t corrupted = *lr_on_stack;
    for (int i = 60; i >= 0; i -= 4)
        uart_putc("0123456789ABCDEF"[(corrupted >> i) & 0xF]);
    uart_putc('\n');

    /* When this function returns, AUTIASP will fail */
}

void vPacDemoTask(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(100));

    uart_puts("\n[PAC] Pointer Authentication demo\n");
    uart_puts("[PAC] Corrupting return address...\n");

    corrupt_return_address();

    /* If we reach here, PAC did NOT catch the corruption */
    uart_puts("[PAC] ERROR: returned without fault!\n");

    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
