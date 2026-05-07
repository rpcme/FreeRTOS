/*
 * SVE2 MATCH Demo — Byte Pattern Search
 *
 * MATCH is SVE2-only: finds which elements in one vector appear
 * anywhere in another vector. Searches a data stream for any of
 * N delimiter/marker bytes simultaneously.
 *
 * Use case: IoT protocol parsing — scan a sensor data packet for
 * frame delimiters, escape characters, or error markers in one pass.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include <stdint.h>

/* Simulated IoT sensor packet with embedded markers */
static const uint8_t sensor_packet[] = {
    /* Header */
    0xAA, 0x55,  /* sync bytes */
    0x01,        /* device ID */
    0x20,        /* payload length */
    /* Payload: temperature readings with occasional error markers (0xFE, 0xFF) */
    0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21,
    0x22, 0x23, 0xFE, 0x25, 0x26, 0x27, 0x28, 0x29,  /* 0xFE = sensor fault */
    0x2A, 0x2B, 0x2C, 0xFF, 0x2E, 0x2F, 0x30, 0x31,  /* 0xFF = overflow */
    0x32, 0x33, 0x34, 0x35, 0x36, 0xFE, 0x38, 0x39,  /* another fault */
    /* Trailer */
    0xAA, 0x55,  /* end sync */
    0x00         /* null terminator for display */
};

#define PACKET_LEN (sizeof(sensor_packet) - 1)

/* Markers to search for */
static const uint8_t markers[] = { 0xAA, 0x55, 0xFE, 0xFF };
#define NUM_MARKERS 4

/* ---- Scalar search ---- */
static int search_scalar(const uint8_t *data, int len,
                         const uint8_t *markers, int nmarkers,
                         uint8_t *found_mask)
{
    int count = 0;
    for (int i = 0; i < len; i++) {
        found_mask[i] = 0;
        for (int m = 0; m < nmarkers; m++) {
            if (data[i] == markers[m]) {
                found_mask[i] = 1;
                count++;
                break;
            }
        }
    }
    return count;
}

/* ---- SVE2 search using MATCH ----
 *
 * MATCH Pd.B, Pg/Z, Zn.B, Zm.B
 *   For each active byte i: Pd[i] = 1 if Zn[i] appears anywhere in Zm
 *
 * We broadcast all marker bytes into Zm, then MATCH tells us which
 * data bytes are markers — all in one instruction per vector chunk.
 */
static int search_sve2(const uint8_t *data, int len,
                       const uint8_t *markers, int nmarkers,
                       uint8_t *found_mask)
{
    int count = 0;

    __asm volatile(
        "mov    x4, #0              \n"
        "mov    x5, %[len]          \n"
        "mov    x10, #0             \n"

        /* Build marker vector: fill z1 with repeating marker pattern */
        "mov    w6, %w[m0]          \n"
        "mov    w7, %w[m1]          \n"
        "mov    w8, %w[m2]          \n"
        "mov    w9, %w[m3]          \n"
        /* Pack 4 markers into a 32-bit value, then broadcast */
        "orr    w6, w6, w7, lsl #8  \n"
        "orr    w6, w6, w8, lsl #16 \n"
        "orr    w6, w6, w9, lsl #24 \n"
        "mov    z1.s, w6            \n"  /* replicate 4-byte pattern across vector */

        "ptrue  p1.b                \n"  /* all-true for MATCH segment size */

    "1:                             \n"
        "whilelt p0.b, w4, w5       \n"
        "b.none 2f                  \n"

        /* Load data chunk */
        "ld1b   z0.b, p0/z, [%[data], x4] \n"

        /* MATCH: p2[i] = 1 if z0[i] appears anywhere in z1 */
        "match  p2.b, p0/z, z0.b, z1.b \n"

        /* Store found mask (1 for match, 0 for no match) */
        "mov    z3.b, #0            \n"
        "mov    z3.b, p2/m, #1      \n"
        "st1b   z3.b, p0, [%[mask], x4] \n"

        /* Count matches */
        "cntp   x6, p0, p2.b        \n"
        "add    x10, x10, x6        \n"

        "incb   x4                  \n"
        "b      1b                  \n"
    "2:                             \n"
        "mov    %[count], x10       \n"
        : [count] "=r" (count)
        : [data] "r" (data), [len] "r" (len), [mask] "r" (found_mask),
          [m0] "r" ((uint32_t)markers[0]), [m1] "r" ((uint32_t)markers[1]),
          [m2] "r" ((uint32_t)markers[2]), [m3] "r" ((uint32_t)markers[3])
        : "x4","x5","x6","x7","x8","x9","x10",
          "z0","z1","z2","z3","p0","p1","p2","memory"
    );
    return count;
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

void vMatchDemoRun(void)
{
    uart_puts("\n--- SVE2: MATCH Byte Search (SVE2-only) ---\n");
    uart_puts("  MATCH is SVE2-only — not available in NEON or SVE1\n\n");
    uart_puts("  Scenario: scan IoT sensor packet for markers\n");
    uart_puts("  Markers: 0xAA (sync), 0x55 (sync), 0xFE (fault), 0xFF (overflow)\n\n");

    uint8_t mask_scalar[PACKET_LEN];
    uint8_t mask_sve2[PACKET_LEN];

    /* Scalar */
    uint64_t t0 = read_cycles();
    int count_s = search_scalar(sensor_packet, PACKET_LEN, markers, NUM_MARKERS, mask_scalar);
    uint64_t t1 = read_cycles();
    uint64_t scalar_cycles = t1 - t0;

    /* SVE2 */
    t0 = read_cycles();
    int count_v = search_sve2(sensor_packet, PACKET_LEN, markers, NUM_MARKERS, mask_sve2);
    t1 = read_cycles();
    uint64_t sve2_cycles = t1 - t0;

    /* Display packet with markers highlighted */
    uart_puts("  Packet (");
    print_dec(PACKET_LEN);
    uart_puts(" bytes):\n  Data: ");
    for (int i = 0; i < (int)PACKET_LEN; i++) {
        char hex[3];
        hex[0] = "0123456789ABCDEF"[sensor_packet[i] >> 4];
        hex[1] = "0123456789ABCDEF"[sensor_packet[i] & 0xF];
        hex[2] = 0;
        uart_putc(hex[0]); uart_putc(hex[1]); uart_putc(' ');
    }
    uart_puts("\n  MATCH:");
    for (int i = 0; i < (int)PACKET_LEN; i++) {
        uart_putc(mask_sve2[i] ? '^' : ' ');
        uart_putc(mask_sve2[i] ? '^' : ' ');
        uart_putc(' ');
    }

    uart_puts("\n\n  Found: ");
    print_dec(count_v);
    uart_puts(" markers");
    uart_puts(count_s == count_v ? " ✓ match\n" : " ✗ MISMATCH\n");

    uart_puts("\n  Scalar: ");
    print_dec(scalar_cycles);
    uart_puts(" cycles\n  SVE2:   ");
    print_dec(sve2_cycles);
    uart_puts(" cycles (MATCH)\n");
}
