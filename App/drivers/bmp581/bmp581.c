#include "drivers/bmp581/bmp581.h"

#include <stdbool.h>
#include <stddef.h>

#include "common/hk_time.h"
#include "common/units.h"

/* ---- registers (BST-BMP581-DS004, §7 register map) ---- */
#define BMP581_REG_CHIP_ID     0x01
#define BMP581_REG_TEMP_XLSB   0x1D   /* 0x1D..0x1F temp, 0x20..0x22 press */
#define BMP581_REG_OSR_CONFIG  0x36
#define BMP581_REG_ODR_CONFIG  0x37
#define BMP581_REG_CMD         0x7E

#define BMP581_CMD_SOFT_RESET  0xB6
#define BMP581_CHIP_ID_A       0x50   /* BMP581 */
#define BMP581_CHIP_ID_B       0x51   /* tolerate a future mask revision */

/* OSR_CONFIG (0x36): bit7 reserved | bit6 press_en | osr_p[5:3] | osr_t[2:0].
 * 0x5B = pressure enabled, osr_p x8, osr_t x8 -> nominal conversion 8.3 ms
 * (datasheet Table 7: OSR_P=8/OSR_T=8 caps NORMAL mode at 120 Hz). The env
 * task samples at 2 Hz, so oversampling costs us nothing and buys pressure
 * noise low enough for the descent/landing vertical-speed thresholds. */
#define BMP581_OSR_CONFIG_VAL  0x5B

/* ODR_CONFIG (0x37): bit7 deep_dis | odr[6:2] | pwr_mode[1:0].
 * deep_dis MUST stay asserted: after a soft reset the part sits in DEEP
 * STANDBY, where configuration writes are dropped, and it sinks back there
 * whenever deep_dis=0 and the ODR is below 5 Hz. */
#define BMP581_DEEP_DIS        0x80
#define BMP581_PWR_MASK        0x03
#define BMP581_PWR_STANDBY     0x00
#define BMP581_PWR_FORCED      0x02

/* Poll budget for one forced conversion: 8 x 4 ms = 32 ms against a nominal
 * 8.3 ms. Completion is read back from the device rather than assumed -- the
 * datasheet clears pwr_mode when FORCED falls back to STANDBY. */
#define BMP581_POLL_STEPS      8
#define BMP581_POLL_STEP_MS    4

hk_status_t hk_bmp581_init(hk_bmp581_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7)
{
    if (dev == NULL || bus == NULL) {
        return HK_ERR_PARAM;
    }
    dev->bus  = bus;
    dev->addr = (addr7 == 0) ? 0x46 : addr7;

    hk_status_t st = hk_i2c_write_reg(bus, dev->addr, BMP581_REG_CMD,
                                      BMP581_CMD_SOFT_RESET);
    if (st != HK_OK) {
        return st;
    }
    hk_delay_ms(5);

    uint8_t id = 0;
    st = hk_i2c_read_reg(bus, dev->addr, BMP581_REG_CHIP_ID, &id, 1);
    if (st != HK_OK) {
        return st;
    }
    if (id != BMP581_CHIP_ID_A && id != BMP581_CHIP_ID_B) {
        return HK_ERR_NOT_FOUND;
    }

    /* Leave DEEP STANDBY for STANDBY before touching any config register. */
    st = hk_i2c_write_reg(bus, dev->addr, BMP581_REG_ODR_CONFIG,
                          BMP581_DEEP_DIS | BMP581_PWR_STANDBY);
    if (st != HK_OK) {
        return st;
    }
    hk_delay_ms(2);

    st = hk_i2c_write_reg(bus, dev->addr, BMP581_REG_OSR_CONFIG,
                          BMP581_OSR_CONFIG_VAL);
    if (st != HK_OK) {
        return st;
    }

    /* Read the oversampling back: a config write swallowed in DEEP STANDBY
     * would otherwise leave the sensor running at 1x forever, silently. */
    uint8_t osr = 0;
    st = hk_i2c_read_reg(bus, dev->addr, BMP581_REG_OSR_CONFIG, &osr, 1);
    if (st != HK_OK) {
        return st;
    }
    if (osr != BMP581_OSR_CONFIG_VAL) {
        return HK_ERR_IO;
    }
    return HK_OK;
}

hk_status_t hk_bmp581_read(hk_bmp581_t *dev, float *temp_c, float *press_pa)
{
    if (dev == NULL || temp_c == NULL || press_pa == NULL) {
        return HK_ERR_PARAM;
    }

    hk_status_t st = hk_i2c_write_reg(dev->bus, dev->addr, BMP581_REG_ODR_CONFIG,
                                      BMP581_DEEP_DIS | BMP581_PWR_FORCED);
    if (st != HK_OK) {
        return st;
    }

    bool done = false;
    for (int i = 0; i < BMP581_POLL_STEPS && !done; ++i) {
        hk_delay_ms(BMP581_POLL_STEP_MS);
        uint8_t odr = 0;
        st = hk_i2c_read_reg(dev->bus, dev->addr, BMP581_REG_ODR_CONFIG, &odr, 1);
        if (st != HK_OK) {
            return st;
        }
        done = ((odr & BMP581_PWR_MASK) == BMP581_PWR_STANDBY);
    }
    if (!done) {
        return HK_ERR_TIMEOUT;
    }

    /* Burst read: the datasheet requires a single burst so the shadow
     * registers stay consistent within one measurement cycle. */
    uint8_t d[6];
    st = hk_i2c_read_reg(dev->bus, dev->addr, BMP581_REG_TEMP_XLSB, d, sizeof(d));
    if (st != HK_OK) {
        return st;
    }

    /* Temperature: signed 24-bit, LSB first, T[C] = raw / 2^16. */
    int32_t t_raw = (int32_t)(((uint32_t)d[2] << 16) | ((uint32_t)d[1] << 8) | d[0]);
    if (t_raw & 0x00800000) {
        t_raw |= (int32_t)0xFF000000;  /* sign-extend */
    }
    *temp_c = (float)t_raw / 65536.0f;

    /* Pressure: unsigned 24-bit, LSB first, p[Pa] = raw / 2^6. */
    uint32_t p_raw = ((uint32_t)d[5] << 16) | ((uint32_t)d[4] << 8) | d[3];
    *press_pa = (float)p_raw / 64.0f;

    return HK_OK;
}

hk_status_t hk_bmp581_read_altitude(hk_bmp581_t *dev, float ref_pa,
                                    float *temp_c, float *press_pa, float *alt_m)
{
    float t, p;
    hk_status_t st = hk_bmp581_read(dev, &t, &p);
    if (st != HK_OK) {
        return st;
    }
    if (temp_c != NULL)   { *temp_c = t; }
    if (press_pa != NULL) { *press_pa = p; }
    if (alt_m != NULL)    { *alt_m = hk_altitude_from_pressure(p, ref_pa); }
    return HK_OK;
}
