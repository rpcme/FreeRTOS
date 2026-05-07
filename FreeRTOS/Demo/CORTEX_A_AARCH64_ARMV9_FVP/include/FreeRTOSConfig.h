/*
 * FreeRTOSConfig.h — CIX P1 / Orion O6
 *
 * AArch64 SRE port running at EL1 (GUEST mode).
 * Single-core, launched from UEFI after ExitBootServices.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Run at EL1 — GUEST is defined via -DGUEST on the compiler command line.
 * The SRE port uses it to select SPSR_EL1/ELR_EL1.
 * QEMU skips the ICC_RPR_EL1 priority check in FreeRTOS_Tick_Handler. */
#define QEMU

/* ---- Scheduler ---- */
#define configUSE_PREEMPTION                1
#define configUSE_TIME_SLICING              1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configMAX_PRIORITIES                8
#define configMINIMAL_STACK_SIZE            512   /* StackType_t = uint64_t, so 4 KB */
#ifdef FVP_PLATFORM
#define configTICK_RATE_HZ                  1000
#else
#endif
#define configUSE_16_BIT_TICKS              0

/* ---- Memory ---- */
#ifdef FVP_PLATFORM
#define configTOTAL_HEAP_SIZE               (64 * 1024)   /* 64 KB — FVP */
#else
#define configTOTAL_HEAP_SIZE               (256 * 1024)  /* 256 KB */
#endif
#define configSUPPORT_STATIC_ALLOCATION     0
#define configSUPPORT_DYNAMIC_ALLOCATION    1

/* ---- Task features ---- */
#define configUSE_MUTEXES                   1
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_QUEUE_SETS                0
#define configUSE_TASK_NOTIFICATIONS        1
#define configUSE_TRACE_FACILITY            0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configUSE_IDLE_HOOK                 1
#define configUSE_TICK_HOOK                 0
#define configUSE_MALLOC_FAILED_HOOK        0
#define configCHECK_FOR_STACK_OVERFLOW      0

/* ---- FPU ---- */
/* 1 = only tasks that use FPU get context saved. */
#define configUSE_TASK_FPU_SUPPORT          1

/* ---- GICv3 interrupt configuration ---- */
/* GIC-700 implements 8 priority bits → 256 unique priorities.
 * However, the SRE port only supports 16/32/64/128/256.
 * GICv3 with security extensions uses the top bit as group,
 * leaving 7 usable bits = 128 priorities for non-secure. */
#define configUNIQUE_INTERRUPT_PRIORITIES   32

/* Priority above which FreeRTOS API calls are not allowed from ISRs.
 * Must be > configUNIQUE_INTERRUPT_PRIORITIES / 2 and <= configUNIQUE_INTERRUPT_PRIORITIES.
 * Lower numeric value = higher priority. */
#define configMAX_API_CALL_INTERRUPT_PRIORITY  24

/* ---- Timer tick setup ---- */
extern void timer_setup_tick(void);
#define configSETUP_TICK_INTERRUPT()    timer_setup_tick()
#define configCLEAR_TICK_INTERRUPT()    /* Timer auto-reloads via TVAL */

/* ---- Assert ---- */
extern void uart_puts(const char *s);
extern void assert_failed(const char *file, int line);
#define configASSERT(x) do { if (!(x)) { assert_failed(__FILE__, __LINE__); } } while(0)

/* ---- Include/exclude API functions ---- */
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

#endif /* FREERTOS_CONFIG_H */
