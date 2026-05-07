/*
 * Armv9 PAC (Pointer Authentication) initialization.
 * Architecture-generic — works on any Armv9 AArch64 implementation.
 *
 * Call from EL1 after SCTLR_EL1.EnIA is set and HCR_EL2.API/APK are enabled.
 * Programs the APIA key pair used by PACIASP/AUTIASP.
 */

    .global pac_init_keys
    .type pac_init_keys, %function
pac_init_keys:
    bti     c
    /* Program PAC APIA key (arbitrary non-zero value).
     * In production, use a hardware RNG to generate per-boot keys. */
    movz    x0, #0xCAFE, lsl #48
    movk    x0, #0xBEEF, lsl #32
    movk    x0, #0xDEAD, lsl #16
    movk    x0, #0xF00D
    msr     APIAKeyLo_EL1, x0

    movz    x0, #0x1234, lsl #48
    movk    x0, #0x5678, lsl #32
    movk    x0, #0x9ABC, lsl #16
    movk    x0, #0xDEF0
    msr     APIAKeyHi_EL1, x0

    isb

    /* Enable PAC: set SCTLR_EL1.EnIA (bit 31) */
    mrs     x0, SCTLR_EL1
    orr     x0, x0, #(1 << 31)
    msr     SCTLR_EL1, x0
    isb

    ret
