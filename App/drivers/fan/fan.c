#include "drivers/fan/fan.h"

#include <stddef.h>

bool hk_fan_thermostat(float temp_c, float on_above_c, float off_below_c, bool current)
{
    if (current) {
        return !(temp_c <= off_below_c);
    }
    return (temp_c >= on_above_c);
}

#if !defined(HK_HOST)

hk_status_t hk_fan_init(hk_fan_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high)
{
    if (dev == NULL || port == NULL) {
        return HK_ERR_PARAM;
    }
    dev->port        = port;
    dev->pin         = pin;
    dev->active_high = active_high;
    dev->on          = false;
    hk_fan_set(dev, false);
    return HK_OK;
}

void hk_fan_set(hk_fan_t *dev, bool on)
{
    dev->on = on;
    GPIO_PinState level = (on == dev->active_high) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(dev->port, dev->pin, level);
}

bool hk_fan_is_on(const hk_fan_t *dev)
{
    return dev->on;
}

#endif /* !HK_HOST */
