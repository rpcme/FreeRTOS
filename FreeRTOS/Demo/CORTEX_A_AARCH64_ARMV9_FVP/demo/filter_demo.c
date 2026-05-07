/*
 * SVE2 Sharpen & Emboss Filters
 *
 * Sharpen kernel:  [ 0 -1  0]      Emboss kernel:  [-2 -1  0]
 *                  [-1  5 -1]                      [-1  1  1]
 *                  [ 0 -1  0]                      [ 0  1  2]
 *
 * Same SVE2 convolution pattern as blur/Sobel.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

extern int pgm_read(const char *path, uint8_t *buf, int *w, int *h);
extern int pgm_write(const char *path, const uint8_t *buf, int w, int h);
extern uint8_t test_image[];

#define MAX_DIM 256
static uint8_t filter_output[MAX_DIM * MAX_DIM];

/* ---- Generic 3x3 convolution (scalar) ---- */
static void convolve_scalar(const uint8_t *in, uint8_t *out, int w, int h,
                            const int kernel[9], int divisor, int bias)
{
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int sum = kernel[0]*in[(y-1)*w+(x-1)] + kernel[1]*in[(y-1)*w+x] + kernel[2]*in[(y-1)*w+(x+1)]
                    + kernel[3]*in[y*w+(x-1)]     + kernel[4]*in[y*w+x]     + kernel[5]*in[y*w+(x+1)]
                    + kernel[6]*in[(y+1)*w+(x-1)] + kernel[7]*in[(y+1)*w+x] + kernel[8]*in[(y+1)*w+(x+1)];
            sum = sum / divisor + bias;
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            out[y*w+x] = (uint8_t)sum;
        }
    }
    for (int x = 0; x < w; x++) { out[x] = in[x]; out[(h-1)*w+x] = in[(h-1)*w+x]; }
    for (int y = 0; y < h; y++) { out[y*w] = in[y*w]; out[y*w+w-1] = in[y*w+w-1]; }
}

/* ---- SVE2 3x3 convolution ---- */
static void convolve_sve2(const uint8_t *in, uint8_t *out, int w, int h,
                          const int kernel[9], int divisor, int bias)
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

            /* Load 9 neighbors into 16-bit lanes */
            "ld1b   z0.h, p0/z, [x6, x9]  \n"
            "ld1b   z1.h, p0/z, [x6, x4]  \n"
            "ld1b   z2.h, p0/z, [x6, x10] \n"
            "ld1b   z3.h, p0/z, [x7, x9]  \n"
            "ld1b   z4.h, p0/z, [x7, x4]  \n"
            "ld1b   z5.h, p0/z, [x7, x10] \n"
            "ld1b   z6.h, p0/z, [x8, x9]  \n"
            "ld1b   z7.h, p0/z, [x8, x4]  \n"
            "ld1b   z8.h, p0/z, [x8, x10] \n"

            /* Multiply by kernel weights and accumulate.
             * Use signed 16-bit arithmetic. Load kernel weights. */
            "mov    z20.h, %w[k0]       \n"
            "mov    z21.h, %w[k1]       \n"
            "mov    z22.h, %w[k2]       \n"
            "mov    z23.h, %w[k3]       \n"
            "mov    z24.h, %w[k4]       \n"
            "mov    z25.h, %w[k5]       \n"
            "mov    z26.h, %w[k6]       \n"
            "mov    z27.h, %w[k7]       \n"
            "mov    z28.h, %w[k8]       \n"

            /* Accumulate: sum = k[i] * pixel[i] */
            "mul    z0.h, z0.h, z20.h   \n"
            "mul    z1.h, z1.h, z21.h   \n"
            "mul    z2.h, z2.h, z22.h   \n"
            "mul    z3.h, z3.h, z23.h   \n"
            "mul    z4.h, z4.h, z24.h   \n"
            "mul    z5.h, z5.h, z25.h   \n"
            "mul    z6.h, z6.h, z26.h   \n"
            "mul    z7.h, z7.h, z27.h   \n"
            "mul    z8.h, z8.h, z28.h   \n"

            "add    z0.h, z0.h, z1.h    \n"
            "add    z0.h, z0.h, z2.h    \n"
            "add    z0.h, z0.h, z3.h    \n"
            "add    z0.h, z0.h, z4.h    \n"
            "add    z0.h, z0.h, z5.h    \n"
            "add    z0.h, z0.h, z6.h    \n"
            "add    z0.h, z0.h, z7.h    \n"
            "add    z0.h, z0.h, z8.h    \n"

            /* Divisor=1 for sharpen/emboss, no division needed */

            /* Add bias */
            "mov    z1.h, %w[bias]      \n"
            "add    z0.h, z0.h, z1.h    \n"

            /* Clamp to [0, 255] */
            "mov    z1.h, #0            \n"
            "smax   z0.h, p0/m, z0.h, z1.h \n"
            "mov    z1.h, #255          \n"
            "smin   z0.h, p0/m, z0.h, z1.h \n"

            /* Store */
            "mov    x9, %[out]          \n"
            "st1b   z0.h, p0, [x9, x4] \n"

            "inch   x4                  \n"
            "whilelt p0.h, w4, w5       \n"
            "b.first 1b                 \n"
        "2:                             \n"
            :
            : [wm1] "r" (w - 1),
              [top] "r" (&in[(y-1)*w]),
              [mid] "r" (&in[y*w]),
              [bot] "r" (&in[(y+1)*w]),
              [out] "r" (&out[y*w]),
              [k0] "r" (kernel[0]), [k1] "r" (kernel[1]), [k2] "r" (kernel[2]),
              [k3] "r" (kernel[3]), [k4] "r" (kernel[4]), [k5] "r" (kernel[5]),
              [k6] "r" (kernel[6]), [k7] "r" (kernel[7]), [k8] "r" (kernel[8]),
              [bias] "r" (bias)
            : "x4","x5","x6","x7","x8","x9","x10",
              "z0","z1","z2","z3","z4","z5","z6","z7","z8",
              "z20","z21","z22","z23","z24","z25","z26","z27","z28",
              "p0","memory"
        );
    }
    for (int x = 0; x < w; x++) { out[x] = in[x]; out[(h-1)*w+x] = in[(h-1)*w+x]; }
    for (int y = 0; y < h; y++) { out[y*w] = in[y*w]; out[y*w+w-1] = in[y*w+w-1]; }
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

static void run_filter(const char *name, const char *outfile,
                       const int kernel[9], int divisor, int bias)
{
    int w = 0, h = 0;
    if (pgm_read("input.pgm", test_image, &w, &h) != 0) {
        uart_puts("  ERROR: cannot read input.pgm\n");
        return;
    }
    uart_puts("  Loaded: input.pgm (");
    print_dec(w); uart_putc('x'); print_dec(h); uart_puts(")\n");

    uint64_t t0 = read_cycles();
    convolve_scalar(test_image, filter_output, w, h, kernel, divisor, bias);
    uint64_t t1 = read_cycles();
    uint64_t scalar_cycles = t1 - t0;

    t0 = read_cycles();
    convolve_sve2(test_image, filter_output, w, h, kernel, divisor, bias);
    t1 = read_cycles();
    uint64_t sve2_cycles = t1 - t0;

    if (pgm_write(outfile, filter_output, w, h) == 0) {
        uart_puts("  Written: ");
        uart_puts(outfile);
        uart_putc('\n');
    }

    uart_puts("\n  Scalar: "); print_dec(scalar_cycles); uart_puts(" cycles\n");
    uart_puts("  SVE2:   "); print_dec(sve2_cycles); uart_puts(" cycles\n");
}

/* Sharpen: [0 -1 0; -1 5 -1; 0 -1 0], divisor=1, bias=0 */
static const int sharpen_kernel[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };

/* Emboss: [-2 -1 0; -1 1 1; 0 1 2], divisor=1, bias=128 */
static const int emboss_kernel[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };

void vSharpenDemoRun(void)
{
    uart_puts("\n--- SVE2: Sharpen Filter (3x3) ---\n");
    uart_puts("  Kernel: [0 -1 0; -1 5 -1; 0 -1 0]\n\n");
    run_filter("Sharpen", "sharpen_output.pgm", sharpen_kernel, 1, 0);
}

void vEmbossDemoRun(void)
{
    uart_puts("\n--- SVE2: Emboss Filter (3x3) ---\n");
    uart_puts("  Kernel: [-2 -1 0; -1 1 1; 0 1 2]\n\n");
    run_filter("Emboss", "emboss_output.pgm", emboss_kernel, 1, 128);
}
