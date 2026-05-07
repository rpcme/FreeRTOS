/*
 * SVE2 Gaussian Blur Demo
 *
 * 3x3 Gaussian kernel:  [1 2 1]
 *                       [2 4 2]  / 16
 *                       [1 2 1]
 *
 * Uses LD1B {Zt.H} (widen-on-load) and ST1B {Zt.H} (narrow-on-store).
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

extern int pgm_read(const char *path, uint8_t *buf, int *w, int *h);
extern int pgm_write(const char *path, const uint8_t *buf, int w, int h);
extern uint8_t test_image[];

#define MAX_DIM 256
static uint8_t blur_output[MAX_DIM * MAX_DIM];

/* ---- Scalar Gaussian 3x3 ---- */
static void gaussian_scalar(const uint8_t *in, uint8_t *out, int w, int h)
{
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int sum = in[(y-1)*w + (x-1)]     + 2*in[(y-1)*w + x]     + in[(y-1)*w + (x+1)]
                    + 2*in[y*w + (x-1)]       + 4*in[y*w + x]         + 2*in[y*w + (x+1)]
                    + in[(y+1)*w + (x-1)]     + 2*in[(y+1)*w + x]     + in[(y+1)*w + (x+1)];
            out[y * w + x] = (uint8_t)(sum >> 4);
        }
    }
    for (int x = 0; x < w; x++) { out[x] = in[x]; out[(h-1)*w + x] = in[(h-1)*w + x]; }
    for (int y = 0; y < h; y++) { out[y*w] = in[y*w]; out[y*w + w-1] = in[y*w + w-1]; }
}

/* ---- SVE2 Gaussian 3x3 ---- */
static void gaussian_sve2(const uint8_t *in, uint8_t *out, int w, int h)
{
    for (int y = 1; y < h - 1; y++) {
        __asm volatile(
            "mov    x4, #1              \n"
            "mov    x5, %[wm1]          \n"
            "whilelt p0.h, w4, w5       \n"
            "b.none 2f                  \n"
        "1:                             \n"
            "mov    x6, %[top]          \n"
            "mov    x7, %[mid]          \n"
            "mov    x8, %[bot]          \n"
            "sub    x9, x4, #1          \n"
            "add    x10, x4, #1         \n"

            /* Load 9 neighbors into 16-bit lanes (free widening) */
            "ld1b   z0.h, p0/z, [x6, x9]  \n"
            "ld1b   z1.h, p0/z, [x6, x4]  \n"
            "ld1b   z2.h, p0/z, [x6, x10] \n"
            "ld1b   z3.h, p0/z, [x7, x9]  \n"
            "ld1b   z4.h, p0/z, [x7, x4]  \n"
            "ld1b   z5.h, p0/z, [x7, x10] \n"
            "ld1b   z6.h, p0/z, [x8, x9]  \n"
            "ld1b   z7.h, p0/z, [x8, x4]  \n"
            "ld1b   z8.h, p0/z, [x8, x10] \n"

            /* Gaussian weights: corners=1, edges=2, center=4 */
            "add    z20.h, z0.h, z2.h   \n"
            "add    z20.h, z20.h, z6.h  \n"
            "add    z20.h, z20.h, z8.h  \n"  /* corners sum */

            "add    z21.h, z1.h, z3.h   \n"
            "add    z21.h, z21.h, z5.h  \n"
            "add    z21.h, z21.h, z7.h  \n"  /* edges sum */
            "lsl    z21.h, z21.h, #1    \n"  /* ×2 */

            "lsl    z22.h, z4.h, #2     \n"  /* center ×4 */

            "add    z20.h, z20.h, z21.h \n"
            "add    z20.h, z20.h, z22.h \n"  /* total */
            "lsr    z20.h, z20.h, #4    \n"  /* ÷16 */

            /* Store (free narrowing: writes low byte of each .h lane) */
            "mov    x9, %[out]          \n"
            "st1b   z20.h, p0, [x9, x4] \n"

            "inch   x4                  \n"
            "whilelt p0.h, w4, w5       \n"
            "b.first 1b                 \n"
        "2:                             \n"
            :
            : [wm1] "r" (w - 1),
              [top] "r" (&in[(y-1)*w]),
              [mid] "r" (&in[y*w]),
              [bot] "r" (&in[(y+1)*w]),
              [out] "r" (&out[y*w])
            : "x4","x5","x6","x7","x8","x9","x10",
              "z0","z1","z2","z3","z4","z5","z6","z7","z8",
              "z20","z21","z22","p0","memory"
        );
    }
    for (int x = 0; x < w; x++) { out[x] = in[x]; out[(h-1)*w + x] = in[(h-1)*w + x]; }
    for (int y = 0; y < h; y++) { out[y*w] = in[y*w]; out[y*w + w-1] = in[y*w + w-1]; }
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

void vBlurDemoRun(void)
{
    uart_puts("\n--- SVE2: Gaussian Blur (3x3) ---\n");

    int w = 0, h = 0;
    if (pgm_read("input.pgm", test_image, &w, &h) == 0) {
        uart_puts("  Loaded: input.pgm (");
        print_dec(w); uart_putc('x'); print_dec(h);
        uart_puts(")\n");
    } else {
        uart_puts("  No input.pgm — using generated gradient\n");
        w = 64; h = 64;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                test_image[y*w + x] = (uint8_t)((x * 255) / w);
    }

    uint64_t t0 = read_cycles();
    gaussian_scalar(test_image, blur_output, w, h);
    uint64_t t1 = read_cycles();
    uint64_t scalar_cycles = t1 - t0;

    t0 = read_cycles();
    gaussian_sve2(test_image, blur_output, w, h);
    t1 = read_cycles();
    uint64_t sve2_cycles = t1 - t0;

    if (pgm_write("blur_output.pgm", blur_output, w, h) == 0)
        uart_puts("  Written: blur_output.pgm\n");

    uart_puts("\n  Scalar: ");
    print_dec(scalar_cycles);
    uart_puts(" cycles\n  SVE2:   ");
    print_dec(sve2_cycles);
    uart_puts(" cycles\n");

    if (scalar_cycles > sve2_cycles) {
        uart_puts("  Speedup: ");
        print_dec(scalar_cycles / sve2_cycles);
        uart_puts("x\n");
    } else {
        uart_puts("  (VL=128: 8 pixels/iter — scales with wider VL)\n");
    }
}
