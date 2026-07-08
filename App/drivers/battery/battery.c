#include "drivers/battery/battery.h"

#include <stddef.h>

#include "common/units.h"

float hk_battery_voltage(uint16_t adc_raw, float vref, float full_scale,
                         float divider_ratio)
{
    if (full_scale <= 0.0f) {
        return 0.0f;
    }
    return ((float)adc_raw / full_scale) * vref * divider_ratio;
}

float hk_battery_soc(float pack_voltage, uint8_t cells)
{
    if (cells == 0) {
        cells = 1;
    }
    float per_cell = pack_voltage / (float)cells;
    float soc = (per_cell - 3.30f) / (4.20f - 3.30f);
    return HK_CLAMP(soc, 0.0f, 1.0f);
}

#if !defined(HK_HOST)

hk_status_t hk_battery_init(hk_battery_t *dev, ADC_HandleTypeDef *hadc, uint32_t channel,
                            float vref, float full_scale, float divider_ratio,
                            uint8_t cells, float ema_alpha)
{
    if (dev == NULL || hadc == NULL) {
        return HK_ERR_PARAM;
    }
    dev->hadc       = hadc;
    dev->channel    = channel;
    dev->vref       = (vref <= 0.0f) ? 3.30f : vref;
    dev->full_scale = (full_scale <= 0.0f) ? 4095.0f : full_scale;
    dev->ratio      = divider_ratio;
    dev->cells      = (cells == 0) ? 3 : cells;
    dev->alpha      = HK_CLAMP(ema_alpha, 0.01f, 1.0f);
    dev->ema        = 0.0f;
    dev->primed     = 0;
    return HK_OK;
}

hk_status_t hk_battery_read(hk_battery_t *dev, float *voltage, float *soc)
{
    if (HAL_ADC_Start(dev->hadc) != HAL_OK) {
        return HK_ERR_IO;
    }
    if (HAL_ADC_PollForConversion(dev->hadc, 10) != HAL_OK) {
        HAL_ADC_Stop(dev->hadc);
        return HK_ERR_TIMEOUT;
    }
    uint16_t raw = (uint16_t)HAL_ADC_GetValue(dev->hadc);
    HAL_ADC_Stop(dev->hadc);

    float v = hk_battery_voltage(raw, dev->vref, dev->full_scale, dev->ratio);

    if (!dev->primed) {
        dev->ema    = v;
        dev->primed = 1;
    } else {
        dev->ema += dev->alpha * (v - dev->ema);
    }

    if (voltage != NULL) { *voltage = dev->ema; }
    if (soc != NULL)     { *soc = hk_battery_soc(dev->ema, dev->cells); }
    return HK_OK;
}

#endif /* !HK_HOST */
