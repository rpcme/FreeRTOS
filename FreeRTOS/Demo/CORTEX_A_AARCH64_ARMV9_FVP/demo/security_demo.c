/*
 * Armv9 Security Demo — Interactive booth demo
 *
 * User types characters into a buffer. Overflow is detected by
 * hardware security features (PAC for stack, MTE for heap).
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

extern volatile uint64_t g_demo_fault_esr;
extern volatile uint64_t g_demo_fault_elr;
extern int demo_setjmp(void);

static void print_hex64(uint64_t val)
{
    for (int i = 60; i >= 0; i -= 4)
        uart_putc("0123456789ABCDEF"[(val >> i) & 0xF]);
}

static void show_menu(void)
{
    uart_puts("\n");
    uart_puts("==================================================\n");
    uart_puts("     ARMv9 for FreeRTOS Demonstration\n");
    uart_puts("==================================================\n\n");
    uart_puts("  [1] PAC — Stack overflow attack (corrupts return address)\n");
    uart_puts("  [2] BTI — Jump to non-landing-pad\n");
    uart_puts("  [3] MTE — Heap buffer overflow\n");
    uart_puts("  [4] SVE2 — Context switch verification\n");
    uart_puts("  [5] SVE2 — Sobel edge detection\n");
    uart_puts("  [6] SVE2 — Histogram (HISTCNT)\n");
    uart_puts("  [7] SVE2 — Gaussian blur\n");
    uart_puts("  [8] SVE2 — SAD motion detection\n");
    uart_puts("  [9] SVE2 — MATCH byte search\n");
    uart_puts("  [A] SVE2 — Sharpen filter\n");
    uart_puts("  [B] SVE2 — Emboss filter\n");
    uart_puts("\n  Type past the buffer boundary to trigger the attack.\n");
    uart_puts("  Press Enter when done typing.\n\n");
    uart_puts("Select demo: ");
}

/* ---- PAC Demo: interactive stack overflow ---- */

static int __attribute__((noinline, optimize("O0")))
pac_interactive_attack(void)
{
    volatile uint8_t buffer[32];

    /* Get pointer to saved LR on stack (at FP+8) */
    uint64_t fp;
    __asm volatile("mov %0, x29" : "=r"(fp));
    volatile uint8_t *lr_ptr = (volatile uint8_t *)(fp + 8);

    /* Show initial state */
    uint64_t orig_lr = 0;
    for (int i = 0; i < 8; i++)
        ((uint8_t *)&orig_lr)[i] = lr_ptr[i];

    uart_puts("  Buffer: 32 bytes at ");
    print_hex64((uint64_t)buffer);
    uart_puts("\n  Saved LR: ");
    print_hex64(orig_lr);
    uart_puts("\n\n  Type text (Enter=done). >32 chars overflows into LR:\n  > ");

    uint32_t count = 0;
    uint32_t lr_offset = 0;
    for (;;) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') break;
        uart_putc(c);

        if (count < 32) {
            buffer[count] = (uint8_t)c;
        } else {
            /* Overflow into saved LR */
            if (lr_offset < 8)
                lr_ptr[lr_offset++] = (uint8_t)c;
        }
        count++;
    }

    uart_puts("\n\n  [");
    if (count >= 10) uart_putc('0' + (count / 10));
    uart_putc('0' + (count % 10));
    uart_puts(" chars typed]\n");

    if (count > 32) {
        uint64_t new_lr = 0;
        for (int i = 0; i < 8; i++)
            ((uint8_t *)&new_lr)[i] = lr_ptr[i];
        uart_puts("  LR corrupted: ");
        print_hex64(new_lr);
        uart_puts("\n  Returning through AUTIASP...\n");
    } else {
        uart_puts("  No overflow — LR intact.\n");
    }

    return 1;
}

static void run_pac_demo(void)
{
    uart_puts("\n--- PAC: Pointer Authentication ---\n");
    uart_puts("  Attack: type >32 chars to overflow buffer into saved return address\n");
    uart_puts("  Defense: AUTIASP verifies PAC signature on function return\n\n");

    g_demo_fault_esr = 0;
    if (demo_setjmp() == 0) {
        pac_interactive_attack();
    }

    if (g_demo_fault_esr != 0) {
        uart_puts("\n  >>> FAULT CAUGHT by PAC! <<<\n");
        uart_puts("  Return address authentication failed.\n");
        uart_puts("  ESR=");
        print_hex64(g_demo_fault_esr);
        uart_puts("\n");
    } else {
        uart_puts("  Function returned safely (no overflow or PAC OK).\n");
    }
}

/* ---- BTI Demo ---- */

static void __attribute__((noinline)) bti_target(void)
{
    uart_puts("OK\n");
}

static void run_bti_demo(void)
{
    uart_puts("\n--- BTI: Branch Target Identification ---\n");
    uart_puts("  Attack: indirect branch skips BTI landing pad\n");
    uart_puts("  Defense: GP pages enforce BTI at branch targets\n\n");

    uart_puts("  Normal indirect call: ");
    void (*fn)(void) = bti_target;
    fn();

    uart_puts("  Calling past BTI landing pad...\n");
    g_demo_fault_esr = 0;
    if (demo_setjmp() == 0) {
        fn = (void (*)(void))((uintptr_t)bti_target + 4);
        fn();
    }

    if (g_demo_fault_esr != 0) {
        uart_puts("\n  >>> FAULT CAUGHT by BTI! <<<\n");
        uart_puts("  Branch target was not a valid landing pad.\n");
        uart_puts("  ESR=");
        print_hex64(g_demo_fault_esr);
        uart_puts("\n");
    } else {
        uart_puts("  ERROR: no fault detected\n");
    }
}

/* ---- MTE Demo: interactive heap overflow ---- */

static void run_mte_demo(void)
{
    uart_puts("\n--- MTE: Memory Tagging Extension ---\n");
    uart_puts("  Attack: type >32 chars to overflow past tagged buffer boundary\n");
    uart_puts("  Defense: adjacent memory has different tag, hardware detects mismatch\n\n");

    /* 64-byte buffer: tag first 32 bytes, leave rest as guard */
    volatile uint8_t buf[64] __attribute__((aligned(16)));
    uint64_t raw = (uint64_t)buf;

    /* Clear all tags to 0 */
    for (uint32_t i = 0; i < 64; i += 16)
        __asm volatile("stg %0, [%0]" :: "r"(raw + i));

    /* Tag first 32 bytes with random tag */
    uint64_t tagged;
    __asm volatile("irg %0, %1" : "=r"(tagged) : "r"(raw));
    __asm volatile("stg %0, [%0]" :: "r"(tagged));
    __asm volatile("stg %0, [%0]" :: "r"(tagged + 16));

    uint8_t tag = (tagged >> 56) & 0xF;
    uart_puts("  Buffer: 32 bytes, tag=");
    uart_putc("0123456789ABCDEF"[tag]);
    uart_puts("  |  Guard: tag=0\n\n");
    uart_puts("  Type text (Enter=done). >32 chars will overflow:\n  > ");

    /* Accumulate input in untagged temp buffer */
    uint8_t input[64];
    uint32_t count = 0;

    for (;;) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') break;
        if (count < 63) {
            uart_putc(c);
            input[count++] = (uint8_t)c;
        }
    }

    uart_puts("\n\n  [");
    if (count >= 10) uart_putc('0' + (count / 10));
    uart_putc('0' + (count % 10));
    uart_puts(" chars] Writing to tagged buffer...\n");

    /* Now write through the tagged pointer — MTE checks each store */
    volatile uint8_t *p = (volatile uint8_t *)tagged;
    g_demo_fault_esr = 0;
    uint32_t written = 0;
    for (written = 0; written < count; written++) {
        p[written] = input[written];
        if (g_demo_fault_esr != 0) break;
    }

    if (g_demo_fault_esr != 0) {
        uart_puts("\n  >>> FAULT CAUGHT by MTE at byte ");
        if ((written + 1) >= 10) uart_putc('0' + ((written + 1) / 10));
        uart_putc('0' + ((written + 1) % 10));
        uart_puts("! <<<\n");
        uart_puts("  Pointer tag (");
        uart_putc("0123456789ABCDEF"[tag]);
        uart_puts(") != memory tag (0)\n");
        uart_puts("  ESR=");
        print_hex64(g_demo_fault_esr);
        uart_puts("\n");
    } else {
        uart_puts("  All bytes written — no overflow detected.\n");
    }
}

extern void vSve2DemoRun(void);
extern void vSobelDemoRun(void);
extern void vHistogramDemoRun(void);
extern void vBlurDemoRun(void);
extern void vSadDemoRun(void);
extern void vMatchDemoRun(void);
extern void vSharpenDemoRun(void);
extern void vEmbossDemoRun(void);

/* ---- Main demo task ---- */
void vSecurityDemoTask(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        show_menu();
        char c = uart_getc();
        uart_putc(c);
        uart_putc('\n');

        switch (c) {
            case '1': run_pac_demo(); break;
            case '2': run_bti_demo(); break;
            case '3': run_mte_demo(); break;
            case '4': vSve2DemoRun(); break;
            case '5': vSobelDemoRun(); break;
            case '6': vHistogramDemoRun(); break;
            case '7': vBlurDemoRun(); break;
            case '8': vSadDemoRun(); break;
            case '9': vMatchDemoRun(); break;
            case 'a': case 'A': vSharpenDemoRun(); break;
            case 'b': case 'B': vEmbossDemoRun(); break;
            default:  uart_puts("  Invalid choice\n"); break;
        }
    }
}
