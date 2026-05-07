/*
 * AArch64 Generic Timer setup for FreeRTOS tick.
 * Uses the VIRTUAL timer (CNTV) at EL1 — PPI 27 (INTID 27).
 */

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS.h"
#include "platform.h"

static uint64_t timer_ticks_per_period;

void timer_setup_tick(void)
{
    uint64_t freq;
    __asm volatile("MRS %0, CNTFRQ_EL0" : "=r"(freq));
    timer_ticks_per_period = freq / configTICK_RATE_HZ;

    __asm volatile("MSR CNTV_TVAL_EL0, %0" :: "r"(timer_ticks_per_period));
    __asm volatile("MSR CNTV_CTL_EL0, %0" :: "r"((uint64_t)1));
}

void timer_irq_handler(void)
{
    /* Reload for next tick */
    __asm volatile("MSR CNTV_TVAL_EL0, %0" :: "r"(timer_ticks_per_period));
    FreeRTOS_Tick_Handler();
}
