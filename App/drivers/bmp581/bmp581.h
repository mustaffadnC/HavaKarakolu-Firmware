#ifndef HK_BMP581_H
#define HK_BMP581_H

#include <stdint.h>

#include "bus/i2c_bus_if.h"
#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bosch BMP581 barometric pressure + temperature sensor (I2C).
 * Register-level driver using FORCED mode: each read triggers one
 * oversampled conversion, waits, then reads the result. Suited to the
 * 1-2 Hz environmental sampling rate.
 *
 * NOTE: register constants verified against the BMP581 datasheet; if a
 * future silicon revision changes them, adjust in bmp581.c.
 */
typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;   /* 0x46 (SDO=GND) or 0x47 */
} hk_bmp581_t;

/* Soft-reset, verify chip id, configure oversampling. */
hk_status_t hk_bmp581_init(hk_bmp581_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7);

/* Trigger forced conversion, wait, read temperature [°C] and pressure [Pa]. */
hk_status_t hk_bmp581_read(hk_bmp581_t *dev, float *temp_c, float *press_pa);

/* Convenience: also compute barometric altitude relative to ref_pa. */
hk_status_t hk_bmp581_read_altitude(hk_bmp581_t *dev, float ref_pa,
                                    float *temp_c, float *press_pa, float *alt_m);

#ifdef __cplusplus
}
#endif

#endif /* HK_BMP581_H */
