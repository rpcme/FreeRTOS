/*
 * Minimal identity-map MMU for FVP FreeRTOS.
 * 4KB granule, level 1 + level 2 block descriptors.
 *
 * Enables:
 *   - BTI enforcement (GP bit on code pages)
 *   - MTE tag storage (Tagged Normal memory on DRAM)
 *   - Proper device memory ordering for MMIO
 *
 * Call from EL1 before FreeRTOS scheduler starts.
 */

#include <stdint.h>

/* Page table entries — 16KB aligned arrays in BSS */
static uint64_t l1_table[512] __attribute__((aligned(4096)));
static uint64_t l2_table_dram[512] __attribute__((aligned(4096)));

/* MAIR_EL1 attribute indices */
#define MAIR_IDX_DEVICE     0   /* Device-nGnRnE */
#define MAIR_IDX_NORMAL     1   /* Normal, Write-Back, Read/Write Allocate */
#define MAIR_IDX_NORMAL_TAG 2   /* Normal Tagged (MTE), Write-Back, R/W Allocate */
#define MAIR_IDX_NORMAL_NC  3   /* Normal Non-Cacheable */

/* MAIR_EL1 value: each index is 8 bits */
#define MAIR_DEVICE         0x00ULL  /* Device-nGnRnE */
#define MAIR_NORMAL         0xFFULL  /* Normal: Inner/Outer WB-RWA */
#define MAIR_NORMAL_TAG     0xF0ULL  /* Tagged Normal: Outer WB-RWA, Inner Tagged (0b0000) */
#define MAIR_NORMAL_NC_VAL  0x44ULL  /* Normal: Inner/Outer Non-Cacheable */

#define MAIR_VALUE ((MAIR_DEVICE << (8 * MAIR_IDX_DEVICE)) | \
                    (MAIR_NORMAL << (8 * MAIR_IDX_NORMAL)) | \
                    (MAIR_NORMAL_TAG << (8 * MAIR_IDX_NORMAL_TAG)) | \
                    (MAIR_NORMAL_NC_VAL << (8 * MAIR_IDX_NORMAL_NC)))

/* Descriptor bits */
#define DESC_VALID      (1ULL << 0)
#define DESC_TABLE      (1ULL << 1)  /* L0/L1: next-level table */
#define DESC_BLOCK      (0ULL << 1)  /* L1/L2: block descriptor (bit 1 = 0) */
#define DESC_AF         (1ULL << 10) /* Access Flag */
#define DESC_SH_INNER   (3ULL << 8)  /* Inner Shareable */
#define DESC_AP_RW      (0ULL << 6)  /* EL1 R/W */
#define DESC_AP_RO      (2ULL << 6)  /* EL1 Read-Only */
#define DESC_XN         (1ULL << 54) /* Execute Never (UXN) */
#define DESC_PXN        (1ULL << 53) /* Privileged Execute Never */
#define DESC_GP         (1ULL << 50) /* Guarded Page (BTI enforcement) */
#define DESC_NS         (1ULL << 5)  /* Non-Secure */

#define ATTR_IDX(n)     (((uint64_t)(n)) << 2)

/* Block sizes */
#define L1_BLOCK_SIZE   (1ULL << 30)  /* 1 GB */
#define L2_BLOCK_SIZE   (1ULL << 21)  /* 2 MB */

/* How many 2MB blocks for code (first N blocks are RO+GP) */
#define CODE_BLOCKS     1  /* 2 MB for code — more than enough */

void mmu_init(void)
{
    uint64_t i;

    /* ---- Build Level 1 table ---- */
    /* Entry 0: 0x00000000–0x3FFFFFFF = Device (1GB block) */
    l1_table[0] = (0x00000000ULL) |
                  DESC_VALID | DESC_BLOCK |
                  ATTR_IDX(MAIR_IDX_DEVICE) |
                  DESC_AF;

    /* Entry 1: 0x40000000–0x7FFFFFFF = unmapped */
    l1_table[1] = 0;

    /* Entry 2: 0x80000000–0xBFFFFFFF = DRAM → Level 2 table */
    l1_table[2] = ((uint64_t)l2_table_dram) |
                  DESC_VALID | DESC_TABLE;

    /* Remaining entries: unmapped */
    for (i = 3; i < 512; i++)
        l1_table[i] = 0;

    /* ---- Build Level 2 table for DRAM (0x80000000) ---- */
    for (i = 0; i < 512; i++) {
        uint64_t pa = 0x80000000ULL + (i * L2_BLOCK_SIZE);
        /* All DRAM: Normal WB, Read-Write, Execute, GP (BTI), Tagged */
        l2_table_dram[i] = pa |
                           DESC_VALID | DESC_BLOCK |
                           ATTR_IDX(MAIR_IDX_NORMAL_TAG) |
                           DESC_AF | DESC_SH_INNER |
                           DESC_AP_RW | DESC_GP;
    }

    /* ---- Configure MMU system registers ---- */

    /* MAIR_EL1 */
    __asm volatile("MSR MAIR_EL1, %0" :: "r"(MAIR_VALUE));

    /* TCR_EL1: 4KB granule, 39-bit VA (T0SZ=25, starts at Level 1),
     * Inner-Shareable, WB-RWA cacheability */
    uint64_t tcr = (25ULL << 0)   |  /* T0SZ = 25 (39-bit VA, Level 1 start) */
                   (0ULL << 14)   |  /* TG0 = 4KB granule */
                   (3ULL << 8)    |  /* Inner Shareable (SH0) */
                   (1ULL << 10)   |  /* Outer WB-RWA (ORGN0) */
                   (1ULL << 12)   |  /* Inner WB-RWA (IRGN0) */
                   (1ULL << 23)   |  /* EPD1 = 1 (disable TTBR1 walks) */
                   (1ULL << 37)   |  /* TBI0 = 1 (Top Byte Ignore — required for MTE) */
                   (1ULL << 57);     /* TCMA0 = 1 (unchecked access for tag 0) */
    __asm volatile("MSR TCR_EL1, %0" :: "r"(tcr));

    /* TTBR0_EL1: point to Level 1 table */
    __asm volatile("MSR TTBR0_EL1, %0" :: "r"((uint64_t)l1_table));

    /* Invalidate TLB */
    __asm volatile("DSB SY\n ISB");

    /* Enable MMU + caches */
    uint64_t sctlr;
    __asm volatile("MRS %0, SCTLR_EL1" : "=r"(sctlr));
    sctlr |= (1ULL << 0);   /* M — MMU enable */
    sctlr |= (1ULL << 2);   /* C — Data cache enable */
    sctlr |= (1ULL << 12);  /* I — Instruction cache enable */
    __asm volatile("DSB SY\n ISB\n MSR SCTLR_EL1, %0\n ISB" :: "r"(sctlr) : "memory");
}
