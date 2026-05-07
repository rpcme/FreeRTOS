    .arch armv9-a+sve2
    .text
    .global sve_ctx_test
    .type sve_ctx_test, %function
sve_ctx_test:
    bti     c
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    /* Simulate: FPSR push + ADDVL + ADDVL + STP (critical nesting) */
    stp     x2, x3, [sp, #-16]!     /* FPSR placeholder */
    addvl   sp, sp, #-32            /* Z-regs space */
    addvl   sp, sp, #-3             /* P-regs space */
    stp     x4, x5, [sp, #-16]!    /* Critical nesting */

    /* Undo */
    ldp     x4, x5, [sp], #16
    addvl   sp, sp, #3
    addvl   sp, sp, #31
    addvl   sp, sp, #1
    ldp     x2, x3, [sp], #16

    /* Check SP == x29 */
    cmp     sp, x29
    b.ne    .Lfail2

    mov     x0, #1
    ldp     x29, x30, [sp], #16
    ret

.Lfail2:
    mov     x0, #0
    mov     sp, x29
    ldp     x29, x30, [sp], #16
    ret
