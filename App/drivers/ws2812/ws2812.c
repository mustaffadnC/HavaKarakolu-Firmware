#include "drivers/ws2812/ws2812.h"

#include <string.h>

static void encode_byte(uint8_t value, uint16_t hi, uint16_t lo, uint16_t *out)
{
    for (int bit = 7; bit >= 0; --bit) {
        *out++ = (value & (1U << bit)) ? hi : lo;
    }
}

void hk_ws2812_encode(uint8_t r, uint8_t g, uint8_t b,
                      uint16_t hi, uint16_t lo, uint16_t *out)
{
    encode_byte(g, hi, lo, &out[0]);   /* GRB order */
    encode_byte(r, hi, lo, &out[8]);
    encode_byte(b, hi, lo, &out[16]);
}

#if !defined(HK_HOST)

hk_status_t hk_ws2812_init(hk_ws2812_t *dev, TIM_HandleTypeDef *htim, uint32_t channel,
                           uint16_t hi_ticks, uint16_t lo_ticks)
{
    if (dev == NULL || htim == NULL) {
        return HK_ERR_PARAM;
    }
    dev->htim    = htim;
    dev->channel = channel;
    dev->hi      = hi_ticks;
    dev->lo      = lo_ticks;
    memset(dev->buf, 0, sizeof(dev->buf));  /* reset slots stay 0 (line low) */
    return HK_OK;
}

hk_status_t hk_ws2812_set(hk_ws2812_t *dev, uint8_t r, uint8_t g, uint8_t b)
{
    hk_ws2812_encode(r, g, b, dev->hi, dev->lo, dev->buf);
    /* reset/latch slots remain zero from init */

    HAL_TIM_PWM_Stop_DMA(dev->htim, dev->channel);  /* ensure idle */
    if (HAL_TIM_PWM_Start_DMA(dev->htim, dev->channel,
                              (uint32_t *)dev->buf, HK_WS2812_BUF_LEN) != HAL_OK) {
        return HK_ERR_IO;
    }
    return HK_OK;
}

#endif /* !HK_HOST */
