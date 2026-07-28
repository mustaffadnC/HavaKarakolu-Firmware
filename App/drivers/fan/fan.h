#ifndef HK_FAN_H
#define HK_FAN_H

#include <stdbool.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hysteresis thermostat (pure/host-testable). Given the current fan state and
 * temperature, returns the next state. on_above_c must be > off_below_c.
 *   - currently OFF: turn ON when temp >= on_above_c
 *   - currently ON : turn OFF when temp <= off_below_c
 */
bool hk_fan_thermostat(float temp_c, float on_above_c, float off_below_c, bool current);

#if !defined(HK_HOST)

#include "main.h"

/*
 * 12 V fan via low-side N-MOSFET (AO3400) on a GPIO. For low-side drive,
 * gate HIGH = fan ON, so active_high = true on this board (PB12/PB13).
 * NOTE: these pins lack a timer OC channel on the F407, so this is on/off
 * control; variable speed would need software PWM or a pin remap.
 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    bool          active_high;
    bool          on;
} hk_fan_t;

hk_status_t hk_fan_init(hk_fan_t *dev, GPIO_TypeDef *port, uint16_t pin, bool active_high);
void        hk_fan_set(hk_fan_t *dev, bool on);
bool        hk_fan_is_on(const hk_fan_t *dev);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_FAN_H */
