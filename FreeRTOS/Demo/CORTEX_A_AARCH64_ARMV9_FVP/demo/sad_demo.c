/*
 * SVE2 SAD (Sum of Absolute Differences) Demo
 *
 * Compares two frames to detect motion. Uses UABD (unsigned absolute
 * difference) — a single instruction that computes |a-b| per element.
 *
 * Workflow: reads input.pgm and reference.pgm, computes per-pixel
 * absolute difference, outputs diff image and total SAD score.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

extern int pgm_read(const char *path, uint8_t *buf, int *w, int *h);
extern int pgm_write(const char *path, const uint8_t *buf, int w, int h);
extern uint8_t test_image[];

#define MAX_DIM 256
static uint8_t ref_image[MAX_DIM * MAX_DIM];
static uint8_t diff_output[MAX_DIM * MAX_DIM];

/* ---- Scalar SAD ---- */
static uint64_t sad_scalar(const uint8_t *a, const uint8_t *b,
                           uint8_t *diff, int n)
{
    uint64_t sum = 0;
    for (int i = 0; i < n; i++) {
        int d = (int)a[i] - (int)b[i];
        if (d < 0) d = -d;
        diff[i] = (uint8_t)d;
        sum += d;
    }
    return sum;
}

/* ---- SVE2 SAD using UABD ---- */
static uint64_t sad_sve2(const uint8_t *a, const uint8_t *b,
                         uint8_t *diff, int n)
{
    uint64_t sum = 0;
    __asm volatile(
        "mov    x4, #0              \n"
        "mov    x5, %[n]            \n"
        "mov    x6, #0              \n"  /* running sum */

    "1:                             \n"
        "whilelt p0.b, w4, w5       \n"
        "b.none 2f                  \n"

        /* Load both frames */
        "ld1b   z0.b, p0/z, [%[a], x4] \n"
        "ld1b   z1.b, p0/z, [%[b], x4] \n"

        /* UABD: unsigned absolute difference (destructive: dst=src1) */
        "movprfx z2, z0             \n"
        "uabd   z2.b, p0/m, z2.b, z1.b \n"

        /* Store diff image */
        "st1b   z2.b, p0, [%[diff], x4] \n"

        /* Accumulate: widen to 16-bit and sum */
        "uaddv  d3, p0, z2.b        \n"
        "fmov   x7, d3              \n"
        "add    x6, x6, x7          \n"

        "incb   x4                  \n"
        "b      1b                  \n"
    "2:                             \n"
        "mov    %[sum], x6          \n"
        : [sum] "=r" (sum)
        : [a] "r" (a), [b] "r" (b), [diff] "r" (diff), [n] "r" (n)
        : "x4","x5","x6","x7",
          "z0","z1","z2","z3","p0","memory"
    );
    return sum;
}

static inline uint64_t read_cycles(void)
{
    uint64_t val;
    __asm volatile("mrs %0, CNTVCT_EL0" : "=r"(val));
    return val;
}

static void print_dec(uint64_t val)
{
    char buf[20];
    int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (--i >= 0) uart_putc(buf[i]);
}

void vSadDemoRun(void)
{
    uart_puts("\n--- SVE2: SAD Motion Detection (UABD) ---\n");

    int w1 = 0, h1 = 0, w2 = 0, h2 = 0;

    if (pgm_read("input.pgm", test_image, &w1, &h1) != 0) {
        uart_puts("  ERROR: cannot read input.pgm\n");
        return;
    }
    if (pgm_read("reference.pgm", ref_image, &w2, &h2) != 0) {
        uart_puts("  No reference.pgm — using input.pgm as both (SAD=0)\n");
        uart_puts("  Tip: capture a frame, copy to reference.pgm, move camera, recapture\n");
        /* Copy input as reference for demo */
        w2 = w1; h2 = h1;
        for (int i = 0; i < w1 * h1; i++) ref_image[i] = test_image[i];
    }

    if (w1 != w2 || h1 != h2) {
        uart_puts("  ERROR: image dimensions don't match\n");
        return;
    }

    int n = w1 * h1;
    uart_puts("  Frame: ");
    print_dec(w1); uart_putc('x'); print_dec(h1);
    uart_puts(" ("); print_dec(n); uart_puts(" pixels)\n");

    /* Scalar */
    uint64_t t0 = read_cycles();
    uint64_t sad_s = sad_scalar(test_image, ref_image, diff_output, n);
    uint64_t t1 = read_cycles();
    uint64_t scalar_cycles = t1 - t0;

    /* SVE2 */
    t0 = read_cycles();
    uint64_t sad_v = sad_sve2(test_image, ref_image, diff_output, n);
    t1 = read_cycles();
    uint64_t sve2_cycles = t1 - t0;

    /* Write diff image */
    if (pgm_write("diff_output.pgm", diff_output, w1, h1) == 0)
        uart_puts("  Written: diff_output.pgm\n");

    /* Results */
    uart_puts("\n  SAD score (total pixel difference):\n");
    uart_puts("    Scalar: "); print_dec(sad_s); uart_putc('\n');
    uart_puts("    SVE2:   "); print_dec(sad_v);
    uart_puts(sad_s == sad_v ? " ✓ match\n" : " ✗ MISMATCH\n");

    uint64_t avg_diff = sad_v / n;
    uart_puts("    Average per-pixel diff: "); print_dec(avg_diff);
    if (avg_diff < 5)
        uart_puts(" (no motion)\n");
    else if (avg_diff < 30)
        uart_puts(" (slight motion)\n");
    else
        uart_puts(" (significant motion!)\n");

    uart_puts("\n  Scalar: ");
    print_dec(scalar_cycles);
    uart_puts(" cycles\n  SVE2:   ");
    print_dec(sve2_cycles);
    uart_puts(" cycles\n");
    if (sve2_cycles > 0 && scalar_cycles > sve2_cycles) {
        uart_puts("  Speedup: ");
        print_dec(scalar_cycles / sve2_cycles);
        uart_puts("x\n");
    }
}
