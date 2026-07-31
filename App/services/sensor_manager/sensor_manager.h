#ifndef HK_SENSOR_MANAGER_H
#define HK_SENSOR_MANAGER_H

#include "common/status.h"

#if !defined(HK_HOST)

#include "bus/i2c_bus_if.h"
#include "common/filters.h"
#include "drivers/bmi270/bmi270_drv.h"
#include "drivers/bmp581/bmp581.h"
#include "drivers/sht4x/sht4x.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Owns the I2C sensors (BMP581 + BMI270 on i2c_main, SHT4x on i2c_sht1 and
 * i2c_sht2) and the attitude filter. Sampling functions read the devices and
 * publish into system_state. GPS and battery are driven by their own tasks.
 */
typedef struct {
    hk_bmp581_t      bmp;
    hk_sht4x_t       sht1;
    hk_sht4x_t       sht2;
    hk_bmi270_t      imu;
    hk_comp_filter_t attitude;
    hk_deriv_lpf_t   vspeed;      /* d(alt)/dt at the env task rate */
    float            baro_ref_pa;
} hk_sensor_mgr_t;

/* Initialise all I2C sensors; sets sensor_ok flags in system_state.
 * i2c_main = I2C1 (BMP581 + BMI270), i2c_sht1 = I2C2, i2c_sht2 = bit-bang. */
hk_status_t hk_sensors_init(hk_sensor_mgr_t *m,
                            const hk_i2c_bus_t *i2c_main,
                            const hk_i2c_bus_t *i2c_sht1,
                            const hk_i2c_bus_t *i2c_sht2);

/* Capture the current pressure as the altitude reference (call when stable). */
void hk_sensors_set_baro_ref(hk_sensor_mgr_t *m);

/* Read BMP581 + both SHT4x and publish to system_state (blocks tens of ms,
 * yielding via the RTOS-aware delay). dt_s = env task period in seconds,
 * used for the vertical-speed derivative. */
void hk_sensors_sample_env(hk_sensor_mgr_t *m, float dt_s);

/* Read BMI270, run the complementary filter (dt seconds), publish attitude. */
void hk_sensors_sample_imu(hk_sensor_mgr_t *m, float dt);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_SENSOR_MANAGER_H */
