#include "drivers/buzzer/buzzer.h"

#include <stddef.h>

hk_status_t hk_buzzer_calc(uint32_t timer_clk_hz, uint32_t freq_hz,
                           uint16_t *psc, uint16_t *arr)
{
    if (timer_clk_hz == 0 || freq_hz == 0 || psc == NULL || arr == NULL) {
        return HK_ERR_PARAM;
    }
    uint32_t period = timer_clk_hz / freq_hz;
    if (period < 2) {
        return HK_ERR_PARAM; /* frequency too high for this clock */
    }
    uint32_t p = (period - 1U) / 65536U;          /* 0-based prescaler */
    uint32_t a = period / (p + 1U);
    if (a < 1U) {
        return HK_ERR_PARAM;
    }
    a -= 1U;
    if (a > 0xFFFFU || p > 0xFFFFU) {
        return HK_ERR_PARAM;
    }
    *psc = (uint16_t)p;
    *arr = (uint16_t)a;
    return HK_OK;
}

#if !defined(HK_HOST)

hk_status_t hk_buzzer_init(hk_buzzer_t *dev, TIM_HandleTypeDef *htim,
                           uint32_t channel, uint32_t timer_clk_hz)
{
    if (dev == NULL || htim == NULL) {
        return HK_ERR_PARAM;
    }
    dev->htim         = htim;
    dev->channel      = channel;
    dev->timer_clk_hz = timer_clk_hz;
    dev->playing      = 0;
    return HK_OK;
}

hk_status_t hk_buzzer_tone(hk_buzzer_t *dev, uint32_t freq_hz)
{
    uint16_t psc, arr;
    hk_status_t st = hk_buzzer_calc(dev->timer_clk_hz, freq_hz, &psc, &arr);
    if (st != HK_OK) {
        return st;
    }
    __HAL_TIM_SET_PRESCALER(dev->htim, psc);
    __HAL_TIM_SET_AUTORELOAD(dev->htim, arr);
    __HAL_TIM_SET_COMPARE(dev->htim, dev->channel, (arr + 1U) / 2U); /* 50% */
    __HAL_TIM_SET_COUNTER(dev->htim, 0);
    if (!dev->playing) {
        if (HAL_TIM_PWM_Start(dev->htim, dev->channel) != HAL_OK) {
            return HK_ERR_IO;
        }
        dev->playing = 1;
    }
    return HK_OK;
}

void hk_buzzer_off(hk_buzzer_t *dev)
{
    if (dev->playing) {
        HAL_TIM_PWM_Stop(dev->htim, dev->channel);
        dev->playing = 0;
    }
}

#endif /* !HK_HOST */
