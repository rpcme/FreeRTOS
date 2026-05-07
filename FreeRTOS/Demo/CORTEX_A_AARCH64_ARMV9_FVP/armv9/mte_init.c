/*
 * Armv9 MTE (Memory Tagging Extension) initialization.
 * Architecture-generic — works on any Armv9 with MTE support.
 *
 * Call from EL1 after HCR_EL2.ATA is set.
 */

#include <stdint.h>

void mte_init(void)
{
    uint64_t sctlr;

    /* Enable MTE in SCTLR_EL1:
     * ATA (bit 43) = 1: enable tag checking at EL1
     * ATA0 (bit 42) = 1: enable tag checking at EL0
     * TCF (bits 41:40) = 0b01: synchronous tag check faults at EL1
     * TCF0 (bits 39:38) = 0b01: synchronous tag check faults at EL0
     */
    __asm volatile("MRS %0, SCTLR_EL1" : "=r"(sctlr));
    sctlr |= (1ULL << 43);   /* ATA */
    sctlr |= (1ULL << 42);   /* ATA0 */
    sctlr |= (1ULL << 40);   /* TCF = sync (bit 40) */
    sctlr &= ~(1ULL << 41);  /* TCF = sync (clear bit 41) */
    sctlr |= (1ULL << 38);   /* TCF0 = sync (bit 38) */
    sctlr &= ~(1ULL << 39);  /* TCF0 = sync (clear bit 39) */
    __asm volatile("MSR SCTLR_EL1, %0\nISB\n" :: "r"(sctlr));

    /* GCR_EL1: configure random tag generation.
     * Exclude tag 0 from random generation (tag 0 = untagged/default).
     * Bits [3:0] = exclude mask. Bit 0 = exclude tag 0. */
    __asm volatile("MSR GCR_EL1, %0\nISB\n" :: "r"((uint64_t)0x1));

    /* RGSR_EL1: random seed for tag generation.
     * Use a fixed seed for reproducibility in demo. */
    __asm volatile("MSR RGSR_EL1, %0\nISB\n" :: "r"((uint64_t)0xBEEF));

    /* TFSR_EL1: clear any pending tag check faults */
    __asm volatile("MSR TFSR_EL1, %0" :: "r"((uint64_t)0));
}

void mte_disable(void)
{
    uint64_t sctlr;
    __asm volatile("MRS %0, SCTLR_EL1" : "=r"(sctlr));
    sctlr &= ~(1ULL << 43);  /* ATA off */
    sctlr &= ~(1ULL << 42);  /* ATA0 off */
    __asm volatile("MSR SCTLR_EL1, %0\nISB\n" :: "r"(sctlr));
}

void mte_enable(void)
{
    uint64_t sctlr;
    __asm volatile("MRS %0, SCTLR_EL1" : "=r"(sctlr));
    sctlr |= (1ULL << 43);   /* ATA on */
    sctlr |= (1ULL << 42);   /* ATA0 on */
    __asm volatile("MSR SCTLR_EL1, %0\nISB\n" :: "r"(sctlr));
}
