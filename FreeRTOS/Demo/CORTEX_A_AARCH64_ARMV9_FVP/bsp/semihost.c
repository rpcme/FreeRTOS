/*
 * semihost.c — ARM semihosting for FVP file I/O
 *
 * Uses HLT #0xF000 (AArch64 semihosting trap).
 * The FVP intercepts this and performs host file operations.
 */

#include <stdint.h>
#include <stddef.h>

/* Semihosting operation numbers */
#define SYS_OPEN   0x01
#define SYS_CLOSE  0x02
#define SYS_WRITE  0x05
#define SYS_READ   0x06
#define SYS_FLEN   0x0C

/* Open modes */
#define MODE_R     0   /* "r" */
#define MODE_W     4   /* "w" */
#define MODE_WB    5   /* "wb" */
#define MODE_RB    1   /* "rb" */

static inline long semihost_call(unsigned long op, void *params)
{
    register unsigned long r0 __asm__("x0") = op;
    register void *r1 __asm__("x1") = params;
    __asm volatile(
        "hlt #0xF000"
        : "+r"(r0)
        : "r"(r1)
        : "memory"
    );
    return (long)r0;
}

int sh_open(const char *path, int mode)
{
    int len = 0;
    while (path[len]) len++;
    unsigned long params[3];
    params[0] = (unsigned long)path;
    params[1] = (unsigned long)mode;
    params[2] = (unsigned long)len;
    return (int)semihost_call(SYS_OPEN, params);
}

int sh_close(int fd)
{
    unsigned long params[1];
    params[0] = (unsigned long)fd;
    return (int)semihost_call(SYS_CLOSE, params);
}

int sh_read(int fd, void *buf, int len)
{
    unsigned long params[3];
    params[0] = (unsigned long)fd;
    params[1] = (unsigned long)buf;
    params[2] = (unsigned long)len;
    long remaining = semihost_call(SYS_READ, params);
    return len - (int)remaining;  /* bytes actually read */
}

int sh_write(int fd, const void *buf, int len)
{
    unsigned long params[3];
    params[0] = (unsigned long)fd;
    params[1] = (unsigned long)buf;
    params[2] = (unsigned long)len;
    long remaining = semihost_call(SYS_WRITE, params);
    return len - (int)remaining;  /* bytes actually written */
}

int sh_flen(int fd)
{
    unsigned long params[1];
    params[0] = (unsigned long)fd;
    return (int)semihost_call(SYS_FLEN, params);
}

/* ---- PGM image helpers ---- */

int pgm_read(const char *path, uint8_t *buf, int *w, int *h)
{
    int fd = sh_open(path, MODE_RB);
    if (fd < 0) return -1;

    /* Read header: "P5\n<w> <h>\n255\n" */
    char hdr[32];
    int n = sh_read(fd, hdr, sizeof(hdr));
    if (n < 8) { sh_close(fd); return -2; }

    /* Parse: skip "P5\n", read w and h */
    int i = 0;
    if (hdr[0] != 'P' || hdr[1] != '5') { sh_close(fd); return -3; }
    i = 3; /* skip "P5\n" */

    /* Parse width */
    int width = 0;
    while (i < n && hdr[i] >= '0' && hdr[i] <= '9')
        width = width * 10 + (hdr[i++] - '0');
    i++; /* skip space */

    /* Parse height */
    int height = 0;
    while (i < n && hdr[i] >= '0' && hdr[i] <= '9')
        height = height * 10 + (hdr[i++] - '0');
    i++; /* skip newline */

    /* Skip "255\n" */
    while (i < n && hdr[i] != '\n') i++;
    i++; /* skip the newline after 255 */

    *w = width;
    *h = height;

    /* Copy any pixel data already in hdr buffer */
    int hdr_pixels = n - i;
    for (int j = 0; j < hdr_pixels; j++)
        buf[j] = (uint8_t)hdr[i + j];

    /* Read remaining pixels */
    int remaining = width * height - hdr_pixels;
    if (remaining > 0)
        sh_read(fd, buf + hdr_pixels, remaining);

    sh_close(fd);
    return 0;
}

int pgm_write(const char *path, const uint8_t *buf, int w, int h)
{
    int fd = sh_open(path, MODE_WB);
    if (fd < 0) return -1;

    /* Write header */
    char hdr[32];
    int i = 0;
    hdr[i++] = 'P'; hdr[i++] = '5'; hdr[i++] = '\n';

    /* Width */
    if (w >= 100) hdr[i++] = '0' + (w / 100);
    if (w >= 10) hdr[i++] = '0' + ((w / 10) % 10);
    hdr[i++] = '0' + (w % 10);
    hdr[i++] = ' ';

    /* Height */
    if (h >= 100) hdr[i++] = '0' + (h / 100);
    if (h >= 10) hdr[i++] = '0' + ((h / 10) % 10);
    hdr[i++] = '0' + (h % 10);
    hdr[i++] = '\n';

    hdr[i++] = '2'; hdr[i++] = '5'; hdr[i++] = '5'; hdr[i++] = '\n';

    sh_write(fd, hdr, i);
    sh_write(fd, buf, w * h);
    sh_close(fd);
    return 0;
}
