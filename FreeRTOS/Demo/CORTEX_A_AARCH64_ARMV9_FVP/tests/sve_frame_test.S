    .arch armv9-a+sve2
    .text
    .global sve_frame_test
    .type sve_frame_test, %function
sve_frame_test:
    bti     c
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    mov     w0, #0xAA
    dup     z0.b, w0
    mov     w0, #0xBB
    dup     z1.b, w0
    ptrue   p0.b

    /* SAVE: FPSR, Z-regs, P-regs */
    mrs     x9, FPSR
    mrs     x10, FPCR
    stp     x9, x10, [sp, #-16]!

    addvl   sp, sp, #-32
    str     z0, [sp, #0, MUL VL]
    str     z1, [sp, #1, MUL VL]

    addvl   sp, sp, #-3
    str     p0, [sp, #0, MUL VL]

    /* Clobber */
    mov     w0, #0xFF
    dup     z0.b, w0
    pfalse  p0.b

    /* RESTORE: P-regs, Z-regs, FPSR */
    ldr     p0, [sp, #0, MUL VL]
    addvl   sp, sp, #3

    ldr     z0, [sp, #0, MUL VL]
    ldr     z1, [sp, #1, MUL VL]
    addvl   sp, sp, #31
    addvl   sp, sp, #1

    ldp     x9, x10, [sp], #16
    msr     FPSR, x9
    msr     FPCR, x10

    /* Verify Z0 == 0xAA */
    mov     w0, #0xAA
    dup     z4.b, w0
    ptrue   p1.b
    cmpeq   p2.b, p1/z, z0.b, z4.b
    cntp    x1, p1, p2.b
    rdvl    x2, #1
    cmp     x1, x2
    b.ne    .Lfail

    mov     x0, #1
    mov     sp, x29
    ldp     x29, x30, [sp], #16
    ret

.Lfail:
    mov     x0, #0
    mov     sp, x29
    ldp     x29, x30, [sp], #16
    ret
