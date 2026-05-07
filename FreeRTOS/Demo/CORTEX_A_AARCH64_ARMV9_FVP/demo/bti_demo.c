/*
 * BTI Demo — Branch Target Identification fault demonstration.
 *
 * With GP (Guarded Page) set in the page tables, indirect branches
 * must land on a BTI instruction. We construct a function pointer
 * that points past the BTI landing pad, triggering a fault.
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

extern void uart_puts(const char *s);
extern void uart_putc(char c);

/* This function has BTI c at entry (compiler inserts it with -mbranch-protection) */
static void __attribute__((noinline)) legitimate_target(void)
{
    uart_puts("[BTI] legitimate_target reached\n");
}

void vBtiDemoTask(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(200));

    uart_puts("\n[BTI] Branch Target Identification demo\n");

    /* First: call via function pointer normally (lands on BTI c) — should work */
    void (*fn)(void) = legitimate_target;
    uart_puts("[BTI] Normal indirect call... ");
    fn();

    /* Now: offset the pointer by 4 bytes to skip past the BTI instruction.
     * The indirect branch will land on a non-BTI instruction → fault. */
    uart_puts("[BTI] Calling past BTI landing pad...\n");
    fn = (void (*)(void))((uintptr_t)legitimate_target + 4);
    fn();

    /* Should not reach here */
    uart_puts("[BTI] ERROR: no fault!\n");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
