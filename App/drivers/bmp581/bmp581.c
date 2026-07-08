#include "drivers/bmp581/bmp581.h"

#include <stddef.h>

#include "common/hk_time.h"
#include "common/units.h"

/* ---- registers ---- */
#define BMP581_REG_CHIP_ID     0x01
#define BMP581_REG_TEMP_XLSB   0x1D   /* 0x1D..0x1F temp, 0x20..0x22 press */
#define BMP581_REG_OSR_CONFIG  0x36
#define BMP581_REG_ODR_CONFIG  0x37
#define BMP581_REG_CMD         0x7E

#define BMP581_CMD_SOFT_RESET  0xB6
#define BMP581_CHIP_ID_A       0x50
#define BMP581_CHIP_ID_B       0x51   /* tolerate rev */

/* OSR_CONFIG: bit6 press_en | osr_p[5:3] | osr_t[2:0]; x8 / x8 + press enable */
#define BMP581_OSR_CONFIG_VAL  0x5B
/* ODR_CONFIG power mode field [1:0]: 0 standby, 1 normal, 2 forced, 3 cont. */
#define BMP581_PWR_FORCED      0x02

hk_status_t hk_bmp581_init(hk_bmp581_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7)
{
    if (dev == NULL || bus == NULL) {
        return HK_ERR_PARAM;
    }
    dev->bus  = bus;
    dev->addr = (addr7 == 0) ? 0x46 : addr7;

    hk_status_t st = hk_i2c_write_reg(bus, dev->addr, BMP581_REG_CMD, BMP581_CMD_SOFT_RESET);
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

    st = hk_i2c_write_reg(bus, dev->addr, BMP581_REG_OSR_CONFIG, BMP581_OSR_CONFIG_VAL);
    if (st != HK_OK) {
        return st;
    }
    return HK_OK;
}

hk_status_t hk_bmp581_read(hk_bmp581_t *dev, float *temp_c, float *press_pa)
{
    if (dev == NULL || temp_c == NULL || press_pa == NULL) {
        return HK_ERR_PARAM;
    }

    /* Trigger one forced conversion. */
    hk_status_t st = hk_i2c_write_reg(dev->bus, dev->addr, BMP581_REG_ODR_CONFIG,
                                      BMP581_PWR_FORCED);
    if (st != HK_OK) {
        return st;
    }
    hk_delay_ms(10);  /* OSR x8: conversion completes well within 10 ms */

    uint8_t d[6];
    st = hk_i2c_read_reg(dev->bus, dev->addr, BMP581_REG_TEMP_XLSB, d, sizeof(d));
    if (st != HK_OK) {
        return st;
    }

    /* Temperature: signed 24-bit, LSB-first, scale 1/2^16 °C. */
    int32_t t_raw = (int32_t)(((uint32_t)d[2] << 16) | ((uint32_t)d[1] << 8) | d[0]);
    if (t_raw & 0x00800000) {
        t_raw |= (int32_t)0xFF000000;  /* sign-extend */
    }
    *temp_c = (float)t_raw / 65536.0f;

    /* Pressure: unsigned 24-bit, LSB-first, scale 1/2^6 Pa. */
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
