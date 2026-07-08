#ifndef HK_WS2812_H
#define HK_WS2812_H

#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HK_WS2812_BITS         24
#define HK_WS2812_RESET_SLOTS  40   /* >= 50 µs low at 1.25 µs/bit for latch */
#define HK_WS2812_BUF_LEN      (HK_WS2812_BITS + HK_WS2812_RESET_SLOTS)

/*
 * Encode one WS2812B pixel into 24 PWM compare values (GRB order, MSB first).
 * Each '1' bit -> hi ticks, each '0' bit -> lo ticks. Pure/host-testable.
 * `out` must hold HK_WS2812_BITS entries.
 */
void hk_ws2812_encode(uint8_t r, uint8_t g, uint8_t b,
                      uint16_t hi, uint16_t lo, uint16_t *out);

#if !defined(HK_HOST)

#include "main.h"

/*
 * Single WS2812B pixel (D6 on PA1/TIM5_CH2, D7 on PA2/TIM5_CH3) via timer PWM
 * + DMA. Suggested ticks at 800 kHz bit rate (1.25 µs), timer @84 MHz, ARR=104:
 *   hi (=1) ~58 ticks (~0.70 µs), lo (=0) ~29 ticks (~0.35 µs).
 * CubeMX: the TIM channel's DMA must use half-word memory & peripheral width.
 */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
    uint16_t           hi;
    uint16_t           lo;
    uint16_t           buf[HK_WS2812_BUF_LEN];
} hk_ws2812_t;

hk_status_t hk_ws2812_init(hk_ws2812_t *dev, TIM_HandleTypeDef *htim, uint32_t channel,
                           uint16_t hi_ticks, uint16_t lo_ticks);
hk_status_t hk_ws2812_set(hk_ws2812_t *dev, uint8_t r, uint8_t g, uint8_t b);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_WS2812_H */
