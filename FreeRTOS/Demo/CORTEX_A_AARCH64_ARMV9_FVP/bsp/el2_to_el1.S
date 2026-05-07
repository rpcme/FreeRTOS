/*
 * EL2 → EL1 transition + AArch64 exception vector table for FreeRTOS.
 * Debug version: each unused vector prints a marker to PL011 UART.
 */

#define UART_BASE   0x040d0000
#define UART_DR     0x000
#define UART_FR     0x018

    .global drop_to_el1
    .global _freertos_vector_table
    .global install_freertos_vectors

/* ---- Macro: print a char to PL011 then hang ---- */
.macro exception_debug char
    mov     x0, #UART_BASE
    movk    x0, #0x040d, lsl #16
1:  ldr     w1, [x0, #UART_FR]
    tbnz    w1, #5, 1b
    mov     w1, #\char
    str     w1, [x0, #UART_DR]
    /* Also print ESR_EL1 */
    mrs     x2, ESR_EL1
    /* Print 8 hex nibbles of ESR */
    mov     x3, #28
2:  lsr     x4, x2, x3
    and     x4, x4, #0xf
    cmp     x4, #10
    blt     3f
    add     x4, x4, #('a' - 10)
    b       4f
3:  add     x4, x4, #'0'
4:  ldr     w1, [x0, #UART_FR]
    tbnz    w1, #5, 4b
    str     w4, [x0, #UART_DR]
    subs    x3, x3, #4
    bge     2b
    /* Print newline */
5:  ldr     w1, [x0, #UART_FR]
    tbnz    w1, #5, 5b
    mov     w1, #'\r'
    str     w1, [x0, #UART_DR]
6:  ldr     w1, [x0, #UART_FR]
    tbnz    w1, #5, 6b
    mov     w1, #'\n'
    str     w1, [x0, #UART_DR]
7:  wfi
    b       7b
.endm

/* ---- EL2 → EL1 drop ---- */
    .type drop_to_el1, %function
drop_to_el1:
    bti     c
    mov     x20, x0
    mov     x21, x1

    mov     x0, #(1 << 31)         /* RW */
    orr     x0, x0, #(1 << 41)    /* API */
    orr     x0, x0, #(1 << 40)    /* APK */
    orr     x0, x0, #(1 << 12)    /* DC — default cacheable (needed for MTE) */
    movk    x0, #0x0100, lsl #48  /* ATA (bit 56) */
    msr     HCR_EL2, x0

    mrs     x0, CNTHCTL_EL2
    orr     x0, x0, #3
    msr     CNTHCTL_EL2, x0
    msr     CNTVOFF_EL2, xzr

    mov     x0, #(3 << 20)
    msr     CPACR_EL1, x0

    mov     x0, xzr
    orr     x0, x0, #(1 << 29)
    orr     x0, x0, #(1 << 28)
    orr     x0, x0, #(1 << 23)
    orr     x0, x0, #(1 << 22)
    orr     x0, x0, #(1 << 11)
    orr     x0, x0, #(1 << 31)    /* EnIA — enable PAC for APIA key */
    orr     x0, x0, #(1 << 36)    /* BT0 — enable BTI for EL0 */
    msr     SCTLR_EL1, x0

    /* PAC keys are programmed by pac_init_keys() in src/armv9/ after ERET */

    mov     x0, #0x3c5
    msr     SPSR_EL2, x0

    msr     ELR_EL2, x20
    msr     SP_EL1, x21

    isb
    eret

/* ---- Install vector table ---- */
    .type install_freertos_vectors, %function
install_freertos_vectors:
    bti     c
    adr     x0, _freertos_vector_table
    msr     VBAR_EL1, x0
    dsb     sy
    isb     sy
    ret

/* ---- AArch64 Exception Vector Table ---- */
    .balign 2048
_freertos_vector_table:

/* Current EL with SP0 */
    .balign 128
    b       sync_handler_sp0        /* Synchronous — dispatch SVC vs fault */
    .balign 128
    b       FreeRTOS_IRQ_Handler    /* IRQ */
    .balign 128
    exception_debug 'F'             /* FIQ */
    .balign 128
    exception_debug 'E'             /* SError */

/* Current EL with SPx */
    .balign 128
    b       sync_handler_spx        /* Synchronous — dispatch SVC vs fault */
    .balign 128
    b       FreeRTOS_IRQ_Handler    /* IRQ (timer tick) */
    .balign 128
    exception_debug 'f'             /* FIQ */
    .balign 128
    exception_debug 'e'             /* SError */

/* Lower EL using AArch64 */
    .balign 128
    exception_debug 'L'             /* Synchronous */
    .balign 128
    exception_debug 'l'             /* IRQ */
    .balign 128
    exception_debug 'q'             /* FIQ */
    .balign 128
    exception_debug 'r'             /* SError */

/* Lower EL using AArch32 */
    .balign 128
    exception_debug 'A'             /* Synchronous */
    .balign 128
    exception_debug 'a'             /* IRQ */
    .balign 128
    exception_debug 'Q'             /* FIQ */
    .balign 128
    exception_debug 'R'             /* SError */

/* ---- Synchronous exception dispatchers ---- */
/* Check ESR_EL1.EC to distinguish SVC (0x15) from faults */

sync_handler_sp0:
    mrs     x0, ESR_EL1
    lsr     x0, x0, #26            /* EC field = bits [31:26] */
    cmp     x0, #0x15              /* EC=0x15 = SVC from AArch64 */
    b.eq    FreeRTOS_SWI_Handler
    b       pac_fault_handler      /* Any other sync exception = fault */

sync_handler_spx:
    mrs     x0, ESR_EL1
    lsr     x0, x0, #26
    cmp     x0, #0x15
    b.eq    FreeRTOS_SWI_Handler
    b       pac_fault_handler

/* ---- PAC Fault Handler with PSCI SYSTEM_RESET ---- */
/* Prints message + ESR, counts down 5 seconds, then resets via PSCI */

pac_fault_handler:
    /* Save ESR for display */
    mrs     x19, ESR_EL1

    /* Print "\n!! PAC FAULT DETECTED !!\n" */
    mov     x0, #UART_BASE
    movk    x0, #0x040d, lsl #16
    mov     x20, x0                 /* x20 = UART base */

    /* Helper: print char in w1 */
.macro pchar ch
91: ldr     w1, [x20, #UART_FR]
    tbnz    w1, #5, 91b
    mov     w1, #\ch
    str     w1, [x20, #UART_DR]
.endm

    pchar '\n'
    pchar '!'
    pchar '!'
    pchar ' '
    pchar 'P'
    pchar 'A'
    pchar 'C'
    pchar ' '
    pchar 'F'
    pchar 'A'
    pchar 'U'
    pchar 'L'
    pchar 'T'
    pchar ' '
    pchar 'D'
    pchar 'E'
    pchar 'T'
    pchar 'E'
    pchar 'C'
    pchar 'T'
    pchar 'E'
    pchar 'D'
    pchar ' '
    pchar '!'
    pchar '!'
    pchar '\n'

    /* Print "ESR=" + hex value */
    pchar 'E'
    pchar 'S'
    pchar 'R'
    pchar '='

    /* Print x19 (ESR) as 8 hex nibbles */
    mov     x3, #28
.Lhex_loop:
    lsr     x4, x19, x3
    and     x4, x4, #0xf
    cmp     x4, #10
    b.lt    .Lhex_digit
    add     x4, x4, #('a' - 10)
    b       .Lhex_out
.Lhex_digit:
    add     x4, x4, #'0'
.Lhex_out:
92: ldr     w1, [x20, #UART_FR]
    tbnz    w1, #5, 92b
    str     w4, [x20, #UART_DR]
    subs    x3, x3, #4
    b.ge    .Lhex_loop

    pchar '\n'

    /* Print "Resetting in 5..." and countdown */
    pchar 'R'
    pchar 'e'
    pchar 's'
    pchar 'e'
    pchar 't'
    pchar ' '
    pchar 'i'
    pchar 'n'
    pchar ' '

    /* Countdown 5..1 using timer for delay */
    mov     x21, #5
.Lcount:
    /* Print digit */
    add     w1, w21, #'0'
93: ldr     w2, [x20, #UART_FR]
    tbnz    w2, #5, 93b
    str     w1, [x20, #UART_DR]

94: ldr     w2, [x20, #UART_FR]
    tbnz    w2, #5, 94b
    mov     w1, #'.'
    str     w1, [x20, #UART_DR]

    /* Delay ~1 second using counter */
    mrs     x0, CNTVCT_EL0
    mrs     x1, CNTFRQ_EL0
    add     x1, x0, x1             /* target = now + freq (1 second) */
.Lwait:
    mrs     x0, CNTVCT_EL0
    cmp     x0, x1
    b.lt    .Lwait

    subs    x21, x21, #1
    b.ne    .Lcount

    pchar '\n'

    /* PSCI SYSTEM_RESET: SMC #0 with function ID 0x84000009 */
    movz    x0, #0x8400, lsl #16
    movk    x0, #0x0009
    smc     #0

    /* Should not reach here */
95: wfi
    b       95b
