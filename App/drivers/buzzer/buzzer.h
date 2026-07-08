#ifndef HK_BUZZER_H
#define HK_BUZZER_H

#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compute timer prescaler + auto-reload for a target tone frequency given the
 * timer input clock. Pure/host-testable. Returns HK_OK and fills psc and arr,
 * or HK_ERR_PARAM if freq is out of range.
 */
hk_status_t hk_buzzer_calc(uint32_t timer_clk_hz, uint32_t freq_hz,
                           uint16_t *psc, uint16_t *arr);

#if !defined(HK_HOST)

#include "main.h"

/* Passive buzzer driven by a 16-bit timer PWM channel (PB14 / TIM12_CH1). */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
    uint32_t           timer_clk_hz;
    int                playing;
} hk_buzzer_t;

hk_status_t hk_buzzer_init(hk_buzzer_t *dev, TIM_HandleTypeDef *htim,
                           uint32_t channel, uint32_t timer_clk_hz);
hk_status_t hk_buzzer_tone(hk_buzzer_t *dev, uint32_t freq_hz);
void        hk_buzzer_off(hk_buzzer_t *dev);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_BUZZER_H */
