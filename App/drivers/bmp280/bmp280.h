#ifndef HK_BMP280_H
#define HK_BMP280_H

#include <stdint.h>

#include "bus/i2c_bus_if.h"
#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bosch BMP280 barometric pressure + temperature sensor (I2C, addr 0x76 with
 * SDO=GND). Forced-mode driver matching the 2 Hz env task: each read triggers
 * one conversion (osrs_t x2, osrs_p x16, IIR coeff 4) and polls the STATUS
 * register until it completes (max ~43 ms at these settings).
 *
 * Compensation uses Bosch's integer routines (datasheet §3.11.3, 32-bit
 * temperature / 64-bit pressure). They are exposed as pure functions so host
 * tests can verify them against the datasheet example without any I2C.
 */

/* Calibration coefficients read from NVM at 0x88..0xA1 (little-endian). */
typedef struct {
    uint16_t dig_t1;
    int16_t  dig_t2, dig_t3;
    uint16_t dig_p1;
    int16_t  dig_p2, dig_p3, dig_p4, dig_p5, dig_p6, dig_p7, dig_p8, dig_p9;
} hk_bmp280_calib_t;

typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;
    hk_bmp280_calib_t   calib;
} hk_bmp280_t;

/* Reset, verify chip ID (0x58; 0x60 = BME280 is rejected), read calibration,
 * set CONFIG (IIR). addr7 == 0 defaults to 0x76. */
hk_status_t hk_bmp280_init(hk_bmp280_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7);

/* One forced conversion; blocks (yielding via hk_delay_ms) until done. */
hk_status_t hk_bmp280_read(hk_bmp280_t *dev, float *temp_c, float *press_pa);

/* Same, plus ISA altitude relative to ref_pa (see common/units.h). Output
 * pointers may be NULL if not needed. */
hk_status_t hk_bmp280_read_altitude(hk_bmp280_t *dev, float ref_pa,
                                    float *temp_c, float *press_pa, float *alt_m);

/* ---- Pure helpers (host-testable, no I/O) ---- */

/* Parse the 24-byte little-endian calibration blob (reg 0x88..0x9F). */
void hk_bmp280_parse_calib(const uint8_t raw[24], hk_bmp280_calib_t *c);

/* Bosch integer temperature compensation. Returns temperature in 0.01 degC
 * (e.g. 2508 = 25.08 degC) and writes t_fine for the pressure step. */
int32_t hk_bmp280_comp_temp(const hk_bmp280_calib_t *c, int32_t adc_t,
                            int32_t *t_fine);

/* Bosch 64-bit integer pressure compensation. Returns pressure in Pa as
 * unsigned Q24.8 (e.g. 25767236 = 100653.27 Pa). Requires t_fine from
 * hk_bmp280_comp_temp(). Returns 0 if the calibration would divide by zero. */
uint32_t hk_bmp280_comp_press(const hk_bmp280_calib_t *c, int32_t adc_p,
                              int32_t t_fine);

#ifdef __cplusplus
}
#endif

#endif /* HK_BMP280_H */
