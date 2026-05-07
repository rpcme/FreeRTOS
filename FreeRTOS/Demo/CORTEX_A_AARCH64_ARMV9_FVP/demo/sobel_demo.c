/*
 * SVE2 Image Processing Demo — Sobel Edge Detection
 *
 * Applies a 3x3 Sobel filter to a 64x64 embedded test image.
 * Compares scalar vs SVE2 performance and outputs ASCII art.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

#define IMG_W 256
#define IMG_H 256

/* ---- Test image: gradient background + white rectangle ---- */
uint8_t test_image[IMG_H * IMG_W];
static uint8_t output_scalar[IMG_H * IMG_W];
static uint8_t output_sve2[IMG_H * IMG_W];

static void generate_test_image(void)
{
    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            /* Horizontal gradient background */
            uint8_t val = (uint8_t)((x * 255) / IMG_W);

            /* White rectangle (high contrast edges) */
            if (y >= 16 && y <= 48 && x >= 16 && x <= 48) {
                if (y == 16 || y == 48 || x == 16 || x == 48)
                    val = 255;  /* border */
                else
                    val = 200;  /* interior */
            }
            test_image[y * IMG_W + x] = val;
        }
    }
}

/* ---- Scalar Sobel 3x3 ---- */
static void sobel_scalar(const uint8_t *in, uint8_t *out, int w, int h)
{
    /* Gx kernel: [-1 0 +1]   Gy kernel: [-1 -2 -1]
     *            [-2 0 +2]              [ 0  0  0]
     *            [-1 0 +1]              [+1 +2 +1] */
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int gx = -in[(y-1)*w + (x-1)] + in[(y-1)*w + (x+1)]
                   - 2*in[y*w + (x-1)]    + 2*in[y*w + (x+1)]
                   - in[(y+1)*w + (x-1)]  + in[(y+1)*w + (x+1)];

            int gy = -in[(y-1)*w + (x-1)] - 2*in[(y-1)*w + x] - in[(y-1)*w + (x+1)]
                   + in[(y+1)*w + (x-1)] + 2*in[(y+1)*w + x] + in[(y+1)*w + (x+1)];

            int mag = gx * gx + gy * gy;
            /* Approximate sqrt with threshold */
            out[y * w + x] = (mag > 4000) ? 255 : 0;
        }
    }
    /* Zero borders */
    for (int x = 0; x < w; x++) { out[x] = 0; out[(h-1)*w + x] = 0; }
    for (int y = 0; y < h; y++) { out[y*w] = 0; out[y*w + w-1] = 0; }
}

/* ---- SVE2 Sobel 3x3 (processes one row at a time with predicated loop) ---- */
static void sobel_sve2(const uint8_t *in, uint8_t *out, int w, int h)
{
    for (int y = 1; y < h - 1; y++) {
        int x = 1;
        /* SVE2 predicated loop over columns */
        __asm volatile(
            "mov    x4, %[x]            \n"  /* x = 1 */
            "mov    x5, %[wm1]          \n"  /* w-1 */
            "whilelt p0.b, w4, w5       \n"  /* predicate for active lanes */
            "b.none 2f                  \n"
        "1:                             \n"
            /* Load 3 rows: top, mid, bot (offset by x-1 for left neighbors) */
            "mov    x6, %[top]          \n"
            "mov    x7, %[mid]          \n"
            "mov    x8, %[bot]          \n"

            /* Load top row: [x-1], [x], [x+1] */
            "ld1b   z0.b, p0/z, [x6, x4] \n"  /* top[x] */
            "sub    x9, x4, #1          \n"
            "ld1b   z1.b, p0/z, [x6, x9] \n"  /* top[x-1] */
            "add    x9, x4, #1          \n"
            "ld1b   z2.b, p0/z, [x6, x9] \n"  /* top[x+1] */

            /* Load mid row */
            "ld1b   z3.b, p0/z, [x7, x4] \n"  /* mid[x] */
            "sub    x9, x4, #1          \n"
            "ld1b   z4.b, p0/z, [x7, x9] \n"  /* mid[x-1] */
            "add    x9, x4, #1          \n"
            "ld1b   z5.b, p0/z, [x7, x9] \n"  /* mid[x+1] */

            /* Load bot row */
            "ld1b   z6.b, p0/z, [x8, x4] \n"  /* bot[x] */
            "sub    x9, x4, #1          \n"
            "ld1b   z7.b, p0/z, [x8, x9] \n"  /* bot[x-1] */
            "add    x9, x4, #1          \n"
            "ld1b   z8.b, p0/z, [x8, x9] \n"  /* bot[x+1] */

            /* Gx = (top[x+1] - top[x-1]) + 2*(mid[x+1] - mid[x-1]) + (bot[x+1] - bot[x-1]) */
            /* Work in 16-bit to avoid overflow */
            "uunpklo z10.h, z2.b        \n"  /* top[x+1] -> 16-bit */
            "uunpklo z11.h, z1.b        \n"  /* top[x-1] -> 16-bit */
            "sub    z12.h, z10.h, z11.h \n"  /* top_diff */

            "uunpklo z10.h, z5.b        \n"  /* mid[x+1] */
            "uunpklo z11.h, z4.b        \n"  /* mid[x-1] */
            "sub    z13.h, z10.h, z11.h \n"  /* mid_diff */
            "add    z13.h, z13.h, z13.h \n"  /* 2*mid_diff */

            "uunpklo z10.h, z8.b        \n"  /* bot[x+1] */
            "uunpklo z11.h, z7.b        \n"  /* bot[x-1] */
            "sub    z14.h, z10.h, z11.h \n"  /* bot_diff */

            "add    z12.h, z12.h, z13.h \n"  /* gx partial */
            "add    z12.h, z12.h, z14.h \n"  /* gx complete */

            /* |gx| approximate: just use absolute value */
            "abs    z12.h, p0/m, z12.h  \n"

            /* Gy = (bot[x-1] + 2*bot[x] + bot[x+1]) - (top[x-1] + 2*top[x] + top[x+1]) */
            "uunpklo z10.h, z7.b        \n"  /* bot[x-1] */
            "uunpklo z11.h, z6.b        \n"  /* bot[x] */
            "uunpklo z13.h, z8.b        \n"  /* bot[x+1] */
            "add    z14.h, z11.h, z11.h \n"  /* 2*bot[x] */
            "add    z14.h, z14.h, z10.h \n"
            "add    z14.h, z14.h, z13.h \n"  /* bot_sum */

            "uunpklo z10.h, z1.b        \n"  /* top[x-1] */
            "uunpklo z11.h, z0.b        \n"  /* top[x] */
            "uunpklo z13.h, z2.b        \n"  /* top[x+1] */
            "add    z15.h, z11.h, z11.h \n"  /* 2*top[x] */
            "add    z15.h, z15.h, z10.h \n"
            "add    z15.h, z15.h, z13.h \n"  /* top_sum */

            "sub    z14.h, z14.h, z15.h \n"  /* gy */
            "abs    z14.h, p0/m, z14.h  \n"

            /* mag = |gx| + |gy| (Manhattan approximation) */
            "add    z12.h, z12.h, z14.h \n"

            /* Threshold: if mag > 30, output 255, else 0 */
            "mov    z15.h, #30          \n"
            "cmpgt  p1.h, p0/z, z12.h, z15.h \n"
            "mov    z16.b, #0           \n"
            "mov    z16.b, p1/m, #255   \n"

            /* Store result */
            "mov    x9, %[out]          \n"
            "st1b   z16.b, p0, [x9, x4] \n"

            /* Advance */
            "incb   x4                  \n"
            "whilelt p0.b, w4, w5       \n"
            "b.first 1b                 \n"
        "2:                             \n"
            :
            : [x] "r" (x),
              [wm1] "r" (w - 1),
              [top] "r" (&in[(y-1)*w]),
              [mid] "r" (&in[y*w]),
              [bot] "r" (&in[(y+1)*w]),
              [out] "r" (&out[y*w])
            : "x4","x5","x6","x7","x8","x9",
              "z0","z1","z2","z3","z4","z5","z6","z7","z8",
              "z10","z11","z12","z13","z14","z15","z16",
              "p0","p1","memory"
        );
    }
    /* Zero borders */
    for (int x = 0; x < w; x++) { out[x] = 0; out[(h-1)*w + x] = 0; }
    for (int y = 0; y < h; y++) { out[y*w] = 0; out[y*w + w-1] = 0; }
}

/* ---- Read cycle counter ---- */
static inline uint64_t read_cycles(void)
{
    uint64_t val;
    __asm volatile("mrs %0, CNTVCT_EL0" : "=r"(val));
    return val;
}

/* ---- Print ASCII art (every other row/col for compact display) ---- */
static void print_ascii(const uint8_t *img, int w, int h)
{
    for (int y = 0; y < h; y += 2) {
        uart_puts("    ");
        for (int x = 0; x < w; x += 1) {
            uart_putc(img[y * w + x] > 128 ? '#' : '.');
        }
        uart_putc('\n');
    }
}

static void print_dec(uint64_t val)
{
    char buf[20];
    int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (--i >= 0) uart_putc(buf[i]);
}

/* ---- Public entry point ---- */
extern int pgm_read(const char *path, uint8_t *buf, int *w, int *h);
extern int pgm_write(const char *path, const uint8_t *buf, int w, int h);

void vSobelDemoRun(void)
{
    uart_puts("\n--- SVE2: Sobel Edge Detection ---\n");

    int w = IMG_W, h = IMG_H;
    int from_file = 0;

    /* Try loading from file via semihosting */
    if (pgm_read("input.pgm", test_image, &w, &h) == 0) {
        uart_puts("  Loaded: input.pgm (");
        print_dec(w); uart_putc('x'); print_dec(h);
        uart_puts(")\n");
        from_file = 1;
    } else {
        uart_puts("  No input.pgm — using generated 64x64 test image\n");
        w = IMG_W; h = IMG_H;
        generate_test_image();
    }

    /* Scalar */
    uint64_t t0 = read_cycles();
    sobel_scalar(test_image, output_scalar, w, h);
    uint64_t t1 = read_cycles();
    uint64_t scalar_cycles = t1 - t0;

    /* SVE2 */
    t0 = read_cycles();
    sobel_sve2(test_image, output_sve2, w, h);
    t1 = read_cycles();
    uint64_t sve2_cycles = t1 - t0;

    /* Write output via semihosting */
    if (pgm_write("output.pgm", output_sve2, w, h) == 0)
        uart_puts("  Written: output.pgm\n");

    /* ASCII art (only for small images) */
    if (w <= 64 && h <= 64) {
        uart_puts("\n  Edge detection result:\n");
        print_ascii(output_sve2, w, h);
    }

    uart_puts("\n  Scalar: ");
    print_dec(scalar_cycles);
    uart_puts(" cycles\n  SVE2:   ");
    print_dec(sve2_cycles);
    uart_puts(" cycles\n");

    if (sve2_cycles > 0) {
        uart_puts("  Speedup: ");
        print_dec(scalar_cycles / sve2_cycles);
        uart_puts("x\n");
    }
}
