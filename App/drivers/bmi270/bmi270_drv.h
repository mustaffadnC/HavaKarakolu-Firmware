#ifndef HK_BMI270_DRV_H
#define HK_BMI270_DRV_H

#include <stdint.h>

#include "bus/i2c_bus_if.h"
#include "common/status.h"
#include "common/units.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bosch BMI270 IMU (accel + gyro) over I2C.
 *
 * BMI270 requires a ~8 KB configuration blob uploaded at init, which ships
 * with the official Bosch BMI270_SensorAPI. To enable this driver:
 *   1) Vendor the Bosch sources into App/drivers/bmi270/vendor/
 *      (bmi2.c, bmi270.c, bmi2.h, bmi270.h, bmi2_defs.h, bmi270_config.c ...)
 *   2) Add that folder to the include path and build.
 *   3) Define HK_USE_BMI270 (e.g. as a compiler symbol).
 *
 * Without HK_USE_BMI270 the functions are stubs returning HK_ERR_NOT_FOUND,
 * so the rest of the firmware still builds. See drivers/README.md.
 *
 * Single instance (one IMU on the board); the Bosch device struct is kept
 * file-static in bmi270_drv.c.
 */
typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;        /* 0x68 (SDO=GND) */
    float               acc_lsb_ms2; /* m/s^2 per LSB */
    float               gyr_lsb_rps; /* rad/s per LSB */
} hk_bmi270_t;

/* Upload config, set ±8g / ±2000 dps @100 Hz, enable accel+gyro. */
hk_status_t hk_bmi270_init(hk_bmi270_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7);

/* Read latest accel [m/s^2] and gyro [rad/s]. */
hk_status_t hk_bmi270_read(hk_bmi270_t *dev, hk_vec3f *accel, hk_vec3f *gyro);

#ifdef __cplusplus
}
#endif

#endif /* HK_BMI270_DRV_H */
