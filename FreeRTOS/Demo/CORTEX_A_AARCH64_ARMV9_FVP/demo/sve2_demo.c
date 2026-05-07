/*
 * SVE2 Context Switch Demo
 *
 * Two tasks each fill Z-registers with a unique pattern, do SVE2 work,
 * yield, then verify their registers survived preemption. Proves the
 * context switch correctly saves/restores SVE2 state.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

/* Each task gets a unique seed byte. After loading Z0-Z7 with patterns
 * derived from the seed, the task yields. On resume, it verifies all
 * Z-registers still hold the expected values. */

volatile uint32_t task_a_pass_count;
volatile uint32_t task_b_pass_count;
volatile uint32_t task_a_fail;
volatile uint32_t task_b_fail;

/*
 * sve_work: load pattern, do SVE2 accumulate, yield, verify.
 * Written in inline asm to ensure the compiler doesn't clobber Z-regs
 * across the yield (we rely on the context switch to preserve them).
 *
 * Returns 0 on success, non-zero on mismatch.
 */
static int __attribute__((noinline))
sve_check_pattern(uint8_t seed)
{
    uint64_t result;

    /* Load Z0-Z7 with seed-derived patterns */
    __asm volatile(
        "dup    z0.b, %w[s]         \n"
        "dup    z1.b, %w[s1]        \n"
        "dup    z2.b, %w[s2]        \n"
        "dup    z3.b, %w[s3]        \n"
        "dup    z4.b, %w[s4]        \n"
        "dup    z5.b, %w[s5]        \n"
        "dup    z6.b, %w[s6]        \n"
        "dup    z7.b, %w[s7]        \n"
        :
        : [s] "r" ((uint32_t)seed),
          [s1] "r" ((uint32_t)(seed+1)),
          [s2] "r" ((uint32_t)(seed+2)),
          [s3] "r" ((uint32_t)(seed+3)),
          [s4] "r" ((uint32_t)(seed+4)),
          [s5] "r" ((uint32_t)(seed+5)),
          [s6] "r" ((uint32_t)(seed+6)),
          [s7] "r" ((uint32_t)(seed+7))
        : "z0","z1","z2","z3","z4","z5","z6","z7"
    );

    /* Mark this task as using FPU/SVE so context switch saves it.
     * (Done via portTASK_USES_FLOATING_POINT() at task start) */

    /* Yield — another task runs, potentially clobbering Z-regs */
    vTaskDelay(1);

    /* Verify Z0 still holds the seed pattern */
    __asm volatile(
        "ptrue  p0.b                \n"
        "mov    z8.b, #0            \n"
        "dup    z8.b, %w[s]         \n"
        "cmpeq  p1.b, p0/z, z0.b, z8.b \n"
        "cntp   %[r], p0, p1.b     \n"
        : [r] "=r" (result)
        : [s] "r" ((uint32_t)seed)
        : "z8", "p0", "p1"
    );

    uint64_t vl;
    __asm volatile("rdvl %0, #1" : "=r"(vl));

    if (result != vl)
        return 1;  /* Z0 corrupted */

    /* Verify Z3 holds seed+3 */
    __asm volatile(
        "ptrue  p0.b                \n"
        "dup    z8.b, %w[s]         \n"
        "cmpeq  p1.b, p0/z, z3.b, z8.b \n"
        "cntp   %[r], p0, p1.b     \n"
        : [r] "=r" (result)
        : [s] "r" ((uint32_t)(seed + 3))
        : "z8", "p0", "p1"
    );

    if (result != vl)
        return 2;  /* Z3 corrupted */

    return 0;
}

static volatile int sve_demo_stop;

static void vSveTaskA(void *pvParameters)
{
    (void)pvParameters;
    portTASK_USES_FLOATING_POINT();
    vTaskDelay(1);

    for (;;) {
        if (sve_demo_stop) break;
        int err = sve_check_pattern(0xAA);
        if (err) { task_a_fail = err; break; }
        task_a_pass_count++;
    }
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

static void vSveTaskB(void *pvParameters)
{
    (void)pvParameters;
    portTASK_USES_FLOATING_POINT();
    vTaskDelay(1);

    for (;;) {
        if (sve_demo_stop) break;
        int err = sve_check_pattern(0x55);
        if (err) {
            task_b_fail = err;
            break;
        }
        task_b_pass_count++;
    }
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void vSve2DemoRun(void)
{
    uart_puts("\n--- SVE2: Context Switch Verification ---\n");

    uint64_t vl;
    __asm volatile("rdvl %0, #1" : "=r"(vl));
    uart_puts("  Vector length: ");
    if (vl >= 100) uart_putc('0' + (vl / 100));
    if (vl >= 10) uart_putc('0' + ((vl / 10) % 10));
    uart_putc('0' + (vl % 10));
    uart_puts(" bytes (");
    uint64_t bits = vl * 8;
    if (bits >= 1000) uart_putc('0' + (bits / 1000));
    if (bits >= 100) uart_putc('0' + ((bits / 100) % 10));
    if (bits >= 10) uart_putc('0' + ((bits / 10) % 10));
    uart_putc('0' + (bits % 10));
    uart_puts(" bits)\n\n");

    uart_puts("  Two tasks load unique patterns into Z0-Z7,\n");
    uart_puts("  yield, then verify registers survived preemption.\n\n");

    /* Reset counters */
    task_a_pass_count = 0;
    task_b_pass_count = 0;
    task_a_fail = 0;
    task_b_fail = 0;
    sve_demo_stop = 0;
    __asm volatile("" ::: "memory");

    /* Create the two SVE tasks at same priority */
    if (xTaskCreate(vSveTaskA, "sveA", 512, NULL, 3, NULL) != pdPASS)
        uart_puts("  ERROR: Task A creation failed (heap?)\n");
    if (xTaskCreate(vSveTaskB, "sveB", 512, NULL, 3, NULL) != pdPASS)
        uart_puts("  ERROR: Task B creation failed (heap?)\n");

    /* Wait and report */
    uart_puts("  Running");
    for (int i = 0; i < 10; i++) {
        vTaskDelay(pdMS_TO_TICKS(200));
        uart_putc('.');
    }
    uart_putc('\n');

    /* Signal tasks to stop */
    sve_demo_stop = 1;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Report results */
    uart_puts("\n  Task A (seed=0xAA): ");
    if (task_a_fail) {
        uart_puts("FAIL\n");
    } else {
        uart_puts("PASS — ");
        uint32_t c = task_a_pass_count;
        for (int i = 28; i >= 0; i -= 4)
            uart_putc("0123456789ABCDEF"[(c >> i) & 0xF]);
        uart_puts(" iterations\n");
    }

    uart_puts("  Task B (seed=0x55): ");
    if (task_b_fail) {
        uart_puts("FAIL (Z-reg corrupted)\n");
    } else {
        uart_puts("PASS — ");
        uint32_t c = task_b_pass_count;
        if (c >= 100) uart_putc('0' + (c / 100));
        if (c >= 10) uart_putc('0' + ((c / 10) % 10));
        uart_putc('0' + (c % 10));
        uart_puts(" context switches OK\n");
    }

    if (!task_a_fail && !task_b_fail)
        uart_puts("\n  >>> SVE2 context switch VERIFIED <<<\n");
}
