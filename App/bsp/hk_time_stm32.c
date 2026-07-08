#include "common/hk_time.h"

#if !defined(HK_HOST)

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

void hk_time_init(void)
{
    /* Enable DWT cycle counter for microsecond delays. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t hk_millis(void)
{
    return HAL_GetTick();
}

void hk_delay_ms(uint32_t ms)
{
    /* After the scheduler starts, yield instead of busy-waiting. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS((ms == 0) ? 1U : ms));
    } else {
        HAL_Delay(ms);
    }
}

void hk_delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < cycles) {
        /* busy-wait */
    }
}

#endif /* !HK_HOST */
