#include "drivers/bmp280/bmp280.h"

#include <stddef.h>

#include "common/hk_time.h"
#include "common/units.h"

/* ---- registers ---- */
#define BMP280_REG_CALIB       0x88   /* 0x88..0x9F: dig_T1..dig_P9 (24 B)   */
#define BMP280_REG_CHIP_ID     0xD0
#define BMP280_REG_RESET       0xE0
#define BMP280_REG_STATUS      0xF3   /* bit3 = measuring                    */
#define BMP280_REG_CTRL_MEAS   0xF4   /* osrs_t[7:5] osrs_p[4:2] mode[1:0]   */
#define BMP280_REG_CONFIG      0xF5   /* t_sb[7:5] filter[4:2] spi3w_en[0]   */
#define BMP280_REG_DATA        0xF7   /* press msb/lsb/xlsb, temp msb/lsb/xlsb */

#define BMP280_CHIP_ID         0x58
#define BMP280_CHIP_ID_BME280  0x60   /* explicitly rejected (different comp) */
#define BMP280_CMD_SOFT_RESET  0xB6

/* osrs_t x2 (010) | osrs_p x16 (101) | forced mode (01) */
#define BMP280_CTRL_MEAS_VAL   0x55
/* IIR filter coeff 4 (filter bits 010); t_sb irrelevant in forced mode */
#define BMP280_CONFIG_VAL      0x08

#define BMP280_STATUS_MEASURING 0x08
/* Max conversion at x16/x2 is ~43.2 ms; poll with margin. */
#define BMP280_CONV_TIMEOUT_MS  60
#define BMP280_CONV_FIRST_WAIT_MS 10
#define BMP280_CONV_POLL_MS     2

/* ---- pure helpers ---- */

static inline uint16_t rd_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline int16_t rd_s16le(const uint8_t *p)
{
    return (int16_t)rd_u16le(p);
}

void hk_bmp280_parse_calib(const uint8_t raw[24], hk_bmp280_calib_t *c)
{
    c->dig_t1 = rd_u16le(&raw[0]);
    c->dig_t2 = rd_s16le(&raw[2]);
    c->dig_t3 = rd_s16le(&raw[4]);
    c->dig_p1 = rd_u16le(&raw[6]);
    c->dig_p2 = rd_s16le(&raw[8]);
    c->dig_p3 = rd_s16le(&raw[10]);
    c->dig_p4 = rd_s16le(&raw[12]);
    c->dig_p5 = rd_s16le(&raw[14]);
    c->dig_p6 = rd_s16le(&raw[16]);
    c->dig_p7 = rd_s16le(&raw[18]);
    c->dig_p8 = rd_s16le(&raw[20]);
    c->dig_p9 = rd_s16le(&raw[22]);
}

/* Bosch reference implementation (datasheet §3.11.3), integer only. */
int32_t hk_bmp280_comp_temp(const hk_bmp280_calib_t *c, int32_t adc_t,
                            int32_t *t_fine)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)c->dig_t1 << 1))) *
                    (int32_t)c->dig_t2) >> 11;
    int32_t var2 = (((((adc_t >> 4) - (int32_t)c->dig_t1) *
                      ((adc_t >> 4) - (int32_t)c->dig_t1)) >> 12) *
                    (int32_t)c->dig_t3) >> 14;
    int32_t fine = var1 + var2;
    if (t_fine != NULL) {
        *t_fine = fine;
    }
    return (fine * 5 + 128) >> 8;
}

uint32_t hk_bmp280_comp_press(const hk_bmp280_calib_t *c, int32_t adc_p,
                              int32_t t_fine)
{
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_p6;
    var2 = var2 + ((var1 * (int64_t)c->dig_p5) << 17);
    var2 = var2 + ((int64_t)c->dig_p4 << 35);
    var1 = ((var1 * var1 * (int64_t)c->dig_p3) >> 8) +
           ((var1 * (int64_t)c->dig_p2) << 12);
    var1 = ((((int64_t)1 << 47) + var1) * (int64_t)c->dig_p1) >> 33;
    if (var1 == 0) {
        return 0;   /* avoid division by zero */
    }
    int64_t p = 1048576 - (int64_t)adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->dig_p8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_p7 << 4);
    return (uint32_t)p;   /* Pa, Q24.8 */
}

/* ---- driver ---- */

hk_status_t hk_bmp280_init(hk_bmp280_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7)
{
    if (dev == NULL || bus == NULL) {
        return HK_ERR_PARAM;
    }
    dev->bus  = bus;
    dev->addr = (addr7 == 0) ? 0x76 : addr7;

    hk_status_t st = hk_i2c_write_reg(bus, dev->addr, BMP280_REG_RESET,
                                      BMP280_CMD_SOFT_RESET);
    if (st != HK_OK) {
        return st;
    }
    hk_delay_ms(5);   /* startup after reset: max 2 ms, margin */

    uint8_t id = 0;
    st = hk_i2c_read_reg(bus, dev->addr, BMP280_REG_CHIP_ID, &id, 1);
    if (st != HK_OK) {
        return st;
    }
    if (id != BMP280_CHIP_ID) {
        /* 0x56/0x57 are engineering samples; production parts read 0x58.
         * 0x60 would be a BME280 (different compensation) -- treat all
         * mismatches as not-found so bring-up surfaces the real part. */
        return HK_ERR_NOT_FOUND;
    }

    uint8_t raw[24];
    st = hk_i2c_read_reg(bus, dev->addr, BMP280_REG_CALIB, raw, sizeof(raw));
    if (st != HK_OK) {
        return st;
    }
    hk_bmp280_parse_calib(raw, &dev->calib);

    /* dig_T1/dig_P1 are never 0 on a real part; catch a blank/garbled NVM. */
    if (dev->calib.dig_t1 == 0 || dev->calib.dig_p1 == 0) {
        return HK_ERR_CRC;
    }

    return hk_i2c_write_reg(bus, dev->addr, BMP280_REG_CONFIG, BMP280_CONFIG_VAL);
}

hk_status_t hk_bmp280_read(hk_bmp280_t *dev, float *temp_c, float *press_pa)
{
    if (dev == NULL || temp_c == NULL || press_pa == NULL) {
        return HK_ERR_PARAM;
    }

    /* Trigger one forced conversion (mode bits return to sleep when done). */
    hk_status_t st = hk_i2c_write_reg(dev->bus, dev->addr, BMP280_REG_CTRL_MEAS,
                                      BMP280_CTRL_MEAS_VAL);
    if (st != HK_OK) {
        return st;
    }

    /* Wait out the typical time, then poll STATUS.measuring until clear. */
    hk_delay_ms(BMP280_CONV_FIRST_WAIT_MS);
    uint32_t waited = BMP280_CONV_FIRST_WAIT_MS;
    for (;;) {
        uint8_t status = 0;
        st = hk_i2c_read_reg(dev->bus, dev->addr, BMP280_REG_STATUS, &status, 1);
        if (st != HK_OK) {
            return st;
        }
        if ((status & BMP280_STATUS_MEASURING) == 0u) {
            break;
        }
        if (waited >= BMP280_CONV_TIMEOUT_MS) {
            return HK_ERR_TIMEOUT;
        }
        hk_delay_ms(BMP280_CONV_POLL_MS);
        waited += BMP280_CONV_POLL_MS;
    }

    uint8_t d[6];
    st = hk_i2c_read_reg(dev->bus, dev->addr, BMP280_REG_DATA, d, sizeof(d));
    if (st != HK_OK) {
        return st;
    }

    /* 20-bit raw values: msb<<12 | lsb<<4 | xlsb>>4 (big-endian registers). */
    int32_t adc_p = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) |
                              ((uint32_t)d[2] >> 4));
    int32_t adc_t = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) |
                              ((uint32_t)d[5] >> 4));

    int32_t t_fine = 0;
    int32_t t_centi = hk_bmp280_comp_temp(&dev->calib, adc_t, &t_fine);
    uint32_t p_q24_8 = hk_bmp280_comp_press(&dev->calib, adc_p, t_fine);
    if (p_q24_8 == 0u) {
        return HK_ERR_CRC;   /* degenerate calibration */
    }

    *temp_c   = (float)t_centi / 100.0f;
    *press_pa = (float)p_q24_8 / 256.0f;
    return HK_OK;
}

hk_status_t hk_bmp280_read_altitude(hk_bmp280_t *dev, float ref_pa,
                                    float *temp_c, float *press_pa, float *alt_m)
{
    float t, p;
    hk_status_t st = hk_bmp280_read(dev, &t, &p);
    if (st != HK_OK) {
        return st;
    }
    if (temp_c != NULL)   { *temp_c = t; }
    if (press_pa != NULL) { *press_pa = p; }
    if (alt_m != NULL)    { *alt_m = hk_altitude_from_pressure(p, ref_pa); }
    return HK_OK;
}
