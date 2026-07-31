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
 * Register-level driver using FORCED mode: each read requests one
 * oversampled conversion, waits for the device to report completion, then
 * burst-reads the result. Suited to the 2 Hz environmental sampling rate.
 *
 * Unlike the BMP280 it replaced, this part linearises and compensates on
 * chip, so there are no calibration coefficients to fetch and no
 * compensation arithmetic here: the data registers already hold
 * T = raw/2^16 [C] and p = raw/2^6 [Pa].
 *
 * Register constants and scaling verified against BST-BMP581-DS004.
 */
typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;   /* 0x46 (SDO=GND) or 0x47 (SDO=VDDIO) */
} hk_bmp581_t;

/* Soft-reset, verify chip id, leave deep standby, configure oversampling
 * and read it back. Returns HK_ERR_NOT_FOUND on a chip-id mismatch. */
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
