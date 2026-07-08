#include "drivers/servo/servo.h"

#include "common/units.h"

uint16_t hk_servo_angle_to_us(float deg, uint16_t min_us, uint16_t max_us)
{
    deg = HK_CLAMP(deg, 0.0f, 180.0f);
    float us = (float)min_us + ((float)max_us - (float)min_us) * (deg / 180.0f);
    return (uint16_t)(us + 0.5f);
}

#if !defined(HK_HOST)

hk_status_t hk_servo_init(hk_servo_t *dev, TIM_HandleTypeDef *htim,
                          uint32_t channel, uint16_t min_us, uint16_t max_us)
{
    if (dev == NULL || htim == NULL) {
        return HK_ERR_PARAM;
    }
    dev->htim    = htim;
    dev->channel = channel;
    dev->min_us  = (min_us == 0) ? 1000 : min_us;
    dev->max_us  = (max_us == 0) ? 2000 : max_us;

    if (HAL_TIM_PWM_Start(htim, channel) != HAL_OK) {
        return HK_ERR_IO;
    }
    return HK_OK;
}

hk_status_t hk_servo_set_us(hk_servo_t *dev, uint16_t us)
{
    us = (uint16_t)HK_CLAMP(us, dev->min_us, dev->max_us);
    __HAL_TIM_SET_COMPARE(dev->htim, dev->channel, us);
    return HK_OK;
}

hk_status_t hk_servo_set_angle(hk_servo_t *dev, float deg)
{
    return hk_servo_set_us(dev, hk_servo_angle_to_us(deg, dev->min_us, dev->max_us));
}

#endif /* !HK_HOST */
