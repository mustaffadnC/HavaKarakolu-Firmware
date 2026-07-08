#ifndef HK_SERVO_H
#define HK_SERVO_H

#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Map an angle [0,180]° to a pulse width [min_us,max_us]. Pure/host-testable. */
uint16_t hk_servo_angle_to_us(float deg, uint16_t min_us, uint16_t max_us);

#if !defined(HK_HOST)

#include "main.h"

/*
 * Hobby servo on a timer PWM channel (PA8 / TIM1_CH1, 50 Hz).
 * CubeMX must configure the timer for 1 µs per tick and a 20 ms period
 * (e.g. PSC so tick=1µs, ARR=19999). Then CCR == pulse width in µs.
 */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
    uint16_t           min_us;
    uint16_t           max_us;
} hk_servo_t;

hk_status_t hk_servo_init(hk_servo_t *dev, TIM_HandleTypeDef *htim,
                          uint32_t channel, uint16_t min_us, uint16_t max_us);
hk_status_t hk_servo_set_us(hk_servo_t *dev, uint16_t us);
hk_status_t hk_servo_set_angle(hk_servo_t *dev, float deg);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_SERVO_H */
