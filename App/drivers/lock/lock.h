#ifndef HK_LOCK_H
#define HK_LOCK_H

#include <stdbool.h>

#include "common/status.h"

#if !defined(HK_HOST)

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Solenoid payload lock on a GPIO (PC13 / LOCK_CTRL -> AO3400 -> solenoid).
 *
 * Whether energising the solenoid LOCKS or RELEASES depends on the mechanism
 * (HARDWARE FINDING — confirm). `engaged_state` is the GPIO level that holds
 * the payload (engaged = locked). Fail-safe default at init should be engaged.
 *
 *   engage()  -> hold payload (locked)
 *   release() -> let payload separate (unlocked)
 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    GPIO_PinState engaged_state;
    bool          engaged;
} hk_lock_t;

hk_status_t hk_lock_init(hk_lock_t *dev, GPIO_TypeDef *port, uint16_t pin,
                         GPIO_PinState engaged_state, bool start_engaged);
void        hk_lock_engage(hk_lock_t *dev);
void        hk_lock_release(hk_lock_t *dev);
bool        hk_lock_is_engaged(const hk_lock_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_LOCK_H */
