/*
 * Bare-metal startup for FVP_Base_RevC-2xAEMvA.
 *
 * FVP loads ELF at 0x80000000 and starts execution at EL3.
 * We drop to EL1, set up stack, and branch to freertos_main.
 */

    .section .text.boot
    .global _start
_start:
    /* Disable all secondary cores — only CPU0 proceeds */
    mrs     x0, MPIDR_EL1
    and     x0, x0, #0xFF
    cbnz    x0, .Lpark

    /* ---- EL3 → EL1 drop ---- */
    /* Set SCR_EL3: RW=1 (AArch64 at EL2/EL1), NS=1, API=1, APK=1, ATA=1 */
    mov     x0, #(1 << 10)         /* RW */
    orr     x0, x0, #(1 << 0)     /* NS */
    orr     x0, x0, #(1 << 17)    /* API — PAC trap disable */
    orr     x0, x0, #(1 << 16)    /* APK — PAC key trap disable */
    orr     x0, x0, #(1 << 26)    /* ATA — MTE allocation tag access */
    msr     SCR_EL3, x0

    /* HCR_EL2: RW=1, API=1, APK=1, ATA=1 */
    mov     x0, #(1 << 31)         /* RW */
    orr     x0, x0, #(1 << 41)    /* API */
    orr     x0, x0, #(1 << 40)    /* APK */
    mov     x1, #1
    lsl     x1, x1, #56            /* ATA (bit 56) */
    orr     x0, x0, x1
    msr     HCR_EL2, x0

    /* CPTR_EL3: set EZ=1 (bit 8) to NOT trap SVE, clear TFP (bit 10) */
    mov     x0, #(1 << 8)       /* EZ=1: no SVE trap */
    msr     CPTR_EL3, x0
    isb

    /* CPTR_EL2: clear TZ (bit 8) and TFP (bit 10) to allow SVE+FP at EL1 */
    msr     CPTR_EL2, xzr
    isb
    /* ZCR_EL2/EL1: set max SVE vector length */
    mov     x0, #0xF
    msr     ZCR_EL2, x0
    msr     ZCR_EL1, x0

    /* Set counter frequency (firmware responsibility, FVP doesn't set it) */
    ldr     x0, =100000000
    msr     CNTFRQ_EL0, x0

    /* Enable timer access from EL1 */
    mrs     x0, CNTHCTL_EL2
    orr     x0, x0, #3
    msr     CNTHCTL_EL2, x0
    msr     CNTVOFF_EL2, xzr

    /* CPACR_EL1: enable FP/SIMD (FPEN bits 21:20) + SVE (ZEN bits 17:16) */
    mov     x0, #(3 << 20) | (3 << 16)
    msr     CPACR_EL1, x0

    /* Enable GIC system register interface at EL3 and EL2 */
    mov     x0, #0xF              /* SRE=1, DFB=1, DIB=1, Enable=1 */
    msr     ICC_SRE_EL3, x0
    msr     ICC_SRE_EL2, x0
    isb

    /* Configure GIC redistributor for CPU0 from EL3 (secure access).
     * GICR base = 0x2f100000, SGI_base = GICR + 0x10000 */
    mov     x0, #0x2f100000

    /* First enable GICD: ARE_S=1, ARE_NS=1, EnableGrp1NS=1, EnableGrp1S=1 */
    mov     x3, #0x2f000000
    mov     w1, #0x37             /* EnableGrp1NS|EnableGrp1S|EnableGrp0|ARE_S|ARE_NS */
    str     w1, [x3]
    dsb     sy

    /* Wake redistributor: clear ProcessorSleep (WAKER bit 1) */
    ldr     w1, [x0, #0x14]
    bic     w1, w1, #(1 << 1)
    str     w1, [x0, #0x14]
1:  ldr     w1, [x0, #0x14]
    tbnz    w1, #2, 1b           /* Wait for ChildrenAsleep to clear */

    /* SGI_base at GICR + 0x10000 */
    add     x2, x0, #0x10000

    /* GICR_IGROUPR0: set PPI 27 (timer) to Group 1 */
    ldr     w1, [x2, #0x80]
    orr     w1, w1, #(1 << 27)
    str     w1, [x2, #0x80]

    /* GICR_IGRPMODR0: clear bit 27 for NS Group 1 (not Secure Group 1) */
    ldr     w1, [x2, #0xD00]
    bic     w1, w1, #(1 << 27)
    str     w1, [x2, #0xD00]

    /* GICR_ISENABLER0: enable PPI 27 */
    mov     w1, #(1 << 27)
    str     w1, [x2, #0x100]

    /* GICR_IPRIORITYR for PPI 27: set priority 0xA0 */
    mov     w1, #0xA0
    strb    w1, [x2, #0x400 + 27]

    /* Enable NS Group 1 interrupts from EL3 */
    mov     x0, #1
    msr     ICC_IGRPEN1_EL3, x0
    isb

    dsb     sy
    isb

    /* SCTLR_EL1: I+C caches off, MMU off, PAC enabled */
    mov     x0, xzr
    orr     x0, x0, #(1 << 29)    /* LSMAOE */
    orr     x0, x0, #(1 << 28)    /* nTLSMD */
    orr     x0, x0, #(1 << 23)    /* SPAN */
    orr     x0, x0, #(1 << 22)    /* EIS */
    orr     x0, x0, #(1 << 11)    /* EOS */
    /* EnIA (bit 31) intentionally NOT set — PAC disabled for context switch debugging */
    msr     SCTLR_EL1, x0

    /* SPSR_EL3: return to EL1h with DAIF masked */
    mov     x0, #0x3c5             /* M[3:0]=0101 (EL1h), DAIF=1111 */
    msr     SPSR_EL3, x0

    /* ELR_EL3: jump to _el1_entry */
    adr     x0, _el1_entry
    msr     ELR_EL3, x0

    isb
    eret

_el1_entry:
    /* Enable UART0 for output */
    mov     x10, #0x1c090000
    mov     w11, #0x301
    str     w11, [x10, #0x30]       /* CR = UARTEN|TXE|RXE */

    /* Set up stack at top of first 64MB of DRAM */
    ldr     x0, =0x84000000
    mov     sp, x0

    /* Zero BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
.Lzero_bss:
    cmp     x0, x1
    b.ge    .Lbss_done
    str     xzr, [x0], #8
    b       .Lzero_bss
.Lbss_done:

    /* Branch to C entry */
    bl      freertos_main

    /* Should not return */
.Lhang:
    wfi
    b       .Lhang

.Lpark:
    wfe
    b       .Lpark

/* ---- Install vector table ---- */
    .global install_freertos_vectors
    .type install_freertos_vectors, %function
install_freertos_vectors:
    adr     x0, _freertos_vector_table
    msr     VBAR_EL1, x0
    dsb     sy
    isb     sy
    ret

/* ---- Synchronous exception dispatchers ---- */
sync_handler_sp0:
    mrs     x0, ESR_EL1
    lsr     x0, x0, #26
    cmp     x0, #0x15
    b.eq    FreeRTOS_SWI_Handler
    b       .Lfault

sync_handler_spx:
    mrs     x0, ESR_EL1
    lsr     x0, x0, #26
    cmp     x0, #0x15
    b.eq    FreeRTOS_SWI_Handler
    b       .Lfault

/* Recoverable fault handler for security demos. */
    .global g_demo_fault_esr
    .global g_demo_fault_elr
    .global g_demo_recovery_sp
    .global g_demo_recovery_elr
.Lfault:
    stp     x0, x1, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!

    mrs     x0, ESR_EL1
    mrs     x1, ELR_EL1

    adrp    x2, g_demo_fault_esr
    str     x0, [x2, :lo12:g_demo_fault_esr]
    adrp    x2, g_demo_fault_elr
    str     x1, [x2, :lo12:g_demo_fault_elr]

    /* Check if recovery point is set */
    adrp    x2, g_demo_recovery_elr
    ldr     x3, [x2, :lo12:g_demo_recovery_elr]
    cbz     x3, .Lfault_advance

    /* Set ELR to recovery point */
    msr     ELR_EL1, x3
    str     xzr, [x2, :lo12:g_demo_recovery_elr]

    /* Clear BTYPE in SPSR to prevent BTI fault on ERET target */
    mrs     x3, SPSR_EL1
    bic     x3, x3, #(3 << 10)
    msr     SPSR_EL1, x3

    /* Restore task SP (SP_EL0) */
    adrp    x2, g_demo_recovery_sp
    ldr     x2, [x2, :lo12:g_demo_recovery_sp]
    msr     SP_EL0, x2

    ldp     x2, x3, [sp], #16
    ldp     x0, x1, [sp], #16
    ERET

.Lfault_advance:
    add     x1, x1, #4
    msr     ELR_EL1, x1
    ldp     x2, x3, [sp], #16
    ldp     x0, x1, [sp], #16
    ERET

    .data
    .align 3
g_demo_fault_esr:    .quad 0
g_demo_fault_elr:    .quad 0
g_demo_recovery_sp:  .quad 0
g_demo_recovery_elr: .quad 0
    .text

/* ---- AArch64 Exception Vector Table ---- */
    .balign 2048
    .global _freertos_vector_table
_freertos_vector_table:

/* Current EL with SP0 */
    .balign 128
    b       sync_handler_sp0
    .balign 128
    b       FreeRTOS_IRQ_Handler
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault

/* Current EL with SPx */
    .balign 128
    b       sync_handler_spx
    .balign 128
    b       FreeRTOS_IRQ_Handler
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault

/* Lower EL using AArch64 */
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault

/* Lower EL using AArch32 */
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault
    .balign 128
    b       .Lfault
