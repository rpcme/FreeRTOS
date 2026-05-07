/*
 * FreeRTOS Armv9 Demo — main entry point.
 * Called from fvp_startup after EL3→EL1 drop.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "platform.h"
#include "gic.h"

extern void timer_irq_handler(void);
extern void install_freertos_vectors(void);
extern void pac_init_keys(void);
extern void mte_init(void);
extern void mmu_init(void);
extern void vPacDemoTask(void *pvParameters);
extern void vBtiDemoTask(void *pvParameters);
extern void vMteDemoTask(void *pvParameters);
extern void vSecurityDemoTask(void *pvParameters);
extern int sve_frame_test(void);
extern int sve_ctx_test(void);
extern void vSve2DemoRun(void);
extern void vSobelDemoRun(void);
extern void vHistogramDemoRun(void);
extern void vBlurDemoRun(void);
extern void vMatchDemoRun(void);
extern void vSharpenDemoRun(void);
extern void vEmbossDemoRun(void);

static void vSve2AutoTask(void *p) { (void)p; vTaskDelay(pdMS_TO_TICKS(200)); vEmbossDemoRun(); for(;;) vTaskDelay(pdMS_TO_TICKS(1000)); }

/* ---- Assert handler ---- */
void assert_failed(const char *file, int line)
{
    (void)file; (void)line;
    volatile uint32_t *uart = (volatile uint32_t *)0x1c090000;
    *uart = '!'; *uart = 'A'; *uart = '!';
    for (;;) __asm volatile("wfi");
}

/* ---- IRQ handler (required by FreeRTOS AArch64 SRE port) ---- */
void vApplicationIRQHandler(uint32_t ulICCIAR)
{
    uint32_t intid = ulICCIAR & 0x3FF;
    if (intid == TIMER_PPI)
        timer_irq_handler();
}

/* ---- Heartbeat task ---- */
static void vHeartbeatTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ---- Idle hook ---- */
void vApplicationIdleHook(void)
{
    __asm volatile("wfi");
}

/* ---- Entry point ---- */
void freertos_main(void)
{
    uart_init();

    pac_init_keys();
    mmu_init();
    mte_init();
    install_freertos_vectors();
    gic_init();
    gic_configure_timer_ppi();

    /* Test SVE frame save/restore */
    uart_puts("SVE frame test: ");
    if (sve_frame_test())
        uart_puts("PASS\n");
    else
        uart_puts("FAIL\n");

    uart_puts("SVE ctx test:   ");
    int ctx_result = sve_ctx_test();
    if (ctx_result == 1) {
        uart_puts("PASS\n");
    } else {
        uart_puts("FAIL sp_off=0x");
        for (int i = 12; i >= 0; i -= 4)
            uart_putc("0123456789ABCDEF"[(ctx_result >> i) & 0xF]);
        uart_putc('\n');
    }

    /* Auto-run SVE2 demo for non-interactive testing */
    extern void vSve2DemoRun(void);
    static const char sve_name[] = "sve2auto";
    xTaskCreate(vSve2AutoTask, sve_name, 4096, NULL, 2, NULL);

    static const char hb_name[] = "heartbeat";
    xTaskCreate(vHeartbeatTask, hb_name, configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    static const char demo_name[] = "security";
    xTaskCreate(vSecurityDemoTask, demo_name, 1024, NULL, 2, NULL);

    vTaskStartScheduler();

    uart_puts("ERR: scheduler returned\n");
    for (;;) __asm volatile("wfi");
}
