#include "services/health/health.h"

bool hk_health_all_alive(uint32_t kicked, uint32_t expected)
{
    return (kicked & expected) == expected;
}

#if !defined(HK_HOST)

#include "FreeRTOS.h"
#include "task.h"

static IWDG_HandleTypeDef *s_iwdg;
static uint32_t            s_expected;
static volatile uint32_t   s_kicked;

void hk_health_init(IWDG_HandleTypeDef *hiwdg, uint32_t expected)
{
    s_iwdg     = hiwdg;
    s_expected = expected;
    s_kicked   = 0;
}

void hk_health_kick(hk_task_flag_t task)
{
    taskENTER_CRITICAL();
    s_kicked |= (uint32_t)task;
    taskEXIT_CRITICAL();
}

uint32_t hk_health_service(void)
{
    taskENTER_CRITICAL();
    uint32_t kicked = s_kicked;
    taskEXIT_CRITICAL();

    uint32_t missing = s_expected & ~kicked;
    if (missing == 0) {
        if (s_iwdg != NULL) {
            (void)HAL_IWDG_Refresh(s_iwdg);
        }
        taskENTER_CRITICAL();
        s_kicked = 0;
        taskEXIT_CRITICAL();
    }
    /* If tasks are missing we deliberately do NOT refresh -> IWDG resets. */
    return missing;
}

const char *hk_health_reset_reason(void)
{
    const char *reason = "POR/unknown";
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        reason = "IWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        reason = "WWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        reason = "SOFT";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) {
        reason = "BOR";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        reason = "PIN";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        reason = "LPWR";
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return reason;
}

#endif /* !HK_HOST */
