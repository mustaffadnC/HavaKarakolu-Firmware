#include "drivers/lock/lock.h"

#if !defined(HK_HOST)

static void apply(hk_lock_t *dev)
{
    GPIO_PinState s = dev->engaged
                          ? dev->engaged_state
                          : ((dev->engaged_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(dev->port, dev->pin, s);
}

hk_status_t hk_lock_init(hk_lock_t *dev, GPIO_TypeDef *port, uint16_t pin,
                         GPIO_PinState engaged_state, bool start_engaged)
{
    if (dev == NULL || port == NULL) {
        return HK_ERR_PARAM;
    }
    dev->port          = port;
    dev->pin           = pin;
    dev->engaged_state = engaged_state;
    dev->engaged       = start_engaged;
    apply(dev);
    return HK_OK;
}

void hk_lock_engage(hk_lock_t *dev)
{
    dev->engaged = true;
    apply(dev);
}

void hk_lock_release(hk_lock_t *dev)
{
    dev->engaged = false;
    apply(dev);
}

bool hk_lock_is_engaged(const hk_lock_t *dev)
{
    return dev->engaged;
}

#endif /* !HK_HOST */
