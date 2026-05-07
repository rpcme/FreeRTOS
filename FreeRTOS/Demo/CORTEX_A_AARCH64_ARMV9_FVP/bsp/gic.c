/*
 * GIC-700 initialization for CIX P1 / Orion O6.
 * Board-specific: GICD/GICR MMIO addresses from platform.h.
 */

#include <stdint.h>
#include "platform.h"
#include "uart.h"

void gic_init(void)
{
    /* Enable GIC CPU interface (system register interface) */
    __asm volatile("MSR s3_0_c12_c12_5, %0" :: "r"((uint64_t)0x7)); /* ICC_SRE_EL1 */
    __asm volatile("ISB");

#ifndef FVP_PLATFORM
    /* Enable GIC Distributor: ARE_NS=1, EnableGrp1NS=1 */
    volatile uint32_t *gicd = (volatile uint32_t *)GICD_BASE;
    uint32_t gicd_ctlr = gicd[0];
    gicd_ctlr |= (1u << 4) | (1u << 1);
    gicd[0] = gicd_ctlr;
    __asm volatile("DSB SY; ISB");
#endif

    /* ICC_PMR_EL1: allow all priorities */
    __asm volatile("MSR s3_0_c4_c6_0, %0" :: "r"((uint64_t)0xFF));

    /* ICC_BPR1_EL1: binary point = 0 */
    __asm volatile("MSR s3_0_c12_c12_3, %0" :: "r"((uint64_t)0));

    /* ICC_IGRPEN1_EL1: enable Group 1 interrupts */
    __asm volatile("MSR s3_0_c12_c12_7, %0" :: "r"((uint64_t)1));
    __asm volatile("ISB");
}

void gic_configure_timer_ppi(void)
{
#ifdef FVP_PLATFORM
    /* On FVP with ARE-fixed-to-one and wakeup-on-reset, PPIs are
     * already enabled and routed. */
    (void)0;
#else
    /* Find our redistributor by matching MPIDR affinity */
    uint64_t mpidr;
    __asm volatile("MRS %0, MPIDR_EL1" : "=r"(mpidr));
    uint32_t aff = ((mpidr >> 8) & 0xFF000000) | (mpidr & 0x00FFFFFF);

    volatile uint32_t *gicr = (volatile uint32_t *)GICR_BASE;
    int found = 0;
    for (int i = 0; i < 12; i++) {
        uint64_t typer = *(volatile uint64_t *)((uint8_t *)gicr + 0x08);
        uint32_t raff = (uint32_t)(typer >> 32);
        if (raff == aff) { found = 1; break; }
        if (typer & (1ULL << 4)) break;
        gicr = (volatile uint32_t *)((uint8_t *)gicr + GICR_STRIDE);
    }

    if (!found) { uart_putc('?'); uart_putc('\n'); return; }

    /* Wake up redistributor */
    volatile uint32_t *waker = (volatile uint32_t *)((uint8_t *)gicr + 0x14);
    *waker &= ~(1u << 1);
    while (*waker & (1u << 2)) ;

    /* SGI_base at GICR + 0x10000 */
    volatile uint32_t *sgi_base = (volatile uint32_t *)((uint8_t *)gicr + 0x10000);

    /* Set timer PPI to Group 1, enable it, set priority */
    sgi_base[0x80 / 4] |= (1u << TIMER_PPI);
    sgi_base[0x100 / 4] = (1u << TIMER_PPI);
    volatile uint8_t *prio = (volatile uint8_t *)sgi_base + 0x400 + TIMER_PPI;
    *prio = 0xE0;

    uart_putc('R'); uart_putc('\n');
#endif /* !FVP_PLATFORM */
}
