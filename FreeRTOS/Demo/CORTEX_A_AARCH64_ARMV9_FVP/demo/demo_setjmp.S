/*
 * demo_setjmp / fault recovery for security demos.
 *
 * demo_setjmp: saves callee-saved state, sets recovery point, returns 0.
 * Fault handler: restores state, returns 1 to the same call site.
 */

    .global demo_setjmp
    .type demo_setjmp, %function
    .align 2
demo_setjmp:
    bti     c

    /* Save callee-saved regs + SP + LR to recovery buffer */
    adrp    x0, demo_jmpbuf
    add     x0, x0, :lo12:demo_jmpbuf
    mov     x1, sp
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    str     x1,       [x0, #96]

    /* Set recovery point: fault handler will ERET to demo_longjmp */
    adrp    x2, g_demo_recovery_elr
    adr     x3, demo_longjmp
    str     x3, [x2, :lo12:g_demo_recovery_elr]
    adrp    x2, g_demo_recovery_sp
    str     x1, [x2, :lo12:g_demo_recovery_sp]

    /* Return 0 (normal path) */
    mov     x0, #0
    ret

/* Fault handler ERETs here */
    .global demo_longjmp
    .type demo_longjmp, %function
    .align 2
demo_longjmp:
    bti     j

    /* Restore callee-saved regs from buffer */
    adrp    x0, demo_jmpbuf
    add     x0, x0, :lo12:demo_jmpbuf
    ldp     x19, x20, [x0, #0]
    ldp     x21, x22, [x0, #16]
    ldp     x23, x24, [x0, #32]
    ldp     x25, x26, [x0, #48]
    ldp     x27, x28, [x0, #64]
    ldp     x29, x30, [x0, #80]
    ldr     x1,       [x0, #96]
    mov     sp, x1

    /* Return 1 (fault path) */
    mov     x0, #1
    ret

    .data
    .align 3
demo_jmpbuf: .space 104
