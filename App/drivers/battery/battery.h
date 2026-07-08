#ifndef HK_BATTERY_H
#define HK_BATTERY_H

#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- pure conversions (host-testable) ---- */

/* ADC count -> pack voltage [V] through the resistor divider. */
float hk_battery_voltage(uint16_t adc_raw, float vref, float full_scale,
                         float divider_ratio);

/* Rough Li-ion state of charge [0..1] from pack voltage and cell count
 * (linear 3.30 V/cell = 0 %, 4.20 V/cell = 100 %, clamped). */
float hk_battery_soc(float pack_voltage, uint8_t cells);

#if !defined(HK_HOST)

#include "main.h"

typedef struct {
    ADC_HandleTypeDef *hadc;
    uint32_t           channel;
    float              vref;
    float              full_scale;
    float              ratio;
    uint8_t            cells;
    float              ema;       /* filtered voltage */
    float              alpha;     /* EMA factor 0..1 */
    int                primed;
} hk_battery_t;

hk_status_t hk_battery_init(hk_battery_t *dev, ADC_HandleTypeDef *hadc, uint32_t channel,
                            float vref, float full_scale, float divider_ratio,
                            uint8_t cells, float ema_alpha);

/* Poll ADC, update filter, output filtered voltage [V] and SoC [0..1]. */
hk_status_t hk_battery_read(hk_battery_t *dev, float *voltage, float *soc);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_BATTERY_H */
