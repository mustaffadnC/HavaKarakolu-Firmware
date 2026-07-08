#include "drivers/sht4x/sht4x.h"

#include "common/crc.h"
#include "common/units.h"

hk_status_t hk_sht4x_init(hk_sht4x_t *dev, const hk_i2c_bus_t *bus,
                          uint8_t addr7, const char *name)
{
    if (dev == NULL || bus == NULL) {
        return HK_ERR_PARAM;
    }
    dev->bus      = bus;
    dev->addr     = (addr7 == 0) ? 0x44 : addr7;
    dev->meas_cmd = HK_SHT4X_CMD_MEAS_HIGH;
    dev->name     = (name != NULL) ? name : "SHT4x";
    return HK_OK;
}

hk_status_t hk_sht4x_soft_reset(hk_sht4x_t *dev)
{
    uint8_t cmd = HK_SHT4X_CMD_SOFT_RESET;
    return hk_i2c_write(dev->bus, dev->addr, &cmd, 1);
}

hk_status_t hk_sht4x_read_serial(hk_sht4x_t *dev, uint32_t *serial)
{
    if (serial == NULL) {
        return HK_ERR_PARAM;
    }
    uint8_t cmd = HK_SHT4X_CMD_READ_SERIAL;
    hk_status_t st = hk_i2c_write(dev->bus, dev->addr, &cmd, 1);
    if (st != HK_OK) {
        return st;
    }
    /* caller is expected to allow ~1 ms; serial response is 6 bytes */
    uint8_t raw[6];
    st = hk_i2c_read(dev->bus, dev->addr, raw, sizeof(raw));
    if (st != HK_OK) {
        return st;
    }
    if (hk_crc8_sensirion(&raw[0], 2) != raw[2] ||
        hk_crc8_sensirion(&raw[3], 2) != raw[5]) {
        return HK_ERR_CRC;
    }
    *serial = ((uint32_t)raw[0] << 24) | ((uint32_t)raw[1] << 16) |
              ((uint32_t)raw[3] << 8)  |  (uint32_t)raw[4];
    return HK_OK;
}

hk_status_t hk_sht4x_trigger(hk_sht4x_t *dev, uint8_t cmd)
{
    if (cmd == 0) {
        cmd = HK_SHT4X_CMD_MEAS_HIGH;
    }
    dev->meas_cmd = cmd;
    return hk_i2c_write(dev->bus, dev->addr, &cmd, 1);
}

hk_status_t hk_sht4x_parse(const uint8_t raw[6], float *temp_c, float *rh_pct)
{
    if (raw == NULL || temp_c == NULL || rh_pct == NULL) {
        return HK_ERR_PARAM;
    }
    if (hk_crc8_sensirion(&raw[0], 2) != raw[2] ||
        hk_crc8_sensirion(&raw[3], 2) != raw[5]) {
        return HK_ERR_CRC;
    }
    uint16_t t_ticks  = (uint16_t)((raw[0] << 8) | raw[1]);
    uint16_t rh_ticks = (uint16_t)((raw[3] << 8) | raw[4]);

    *temp_c = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);

    float rh = -6.0f + 125.0f * ((float)rh_ticks / 65535.0f);
    *rh_pct  = HK_CLAMP(rh, 0.0f, 100.0f);
    return HK_OK;
}

hk_status_t hk_sht4x_fetch(hk_sht4x_t *dev, float *temp_c, float *rh_pct)
{
    uint8_t raw[6];
    hk_status_t st = hk_i2c_read(dev->bus, dev->addr, raw, sizeof(raw));
    if (st != HK_OK) {
        return st;
    }
    return hk_sht4x_parse(raw, temp_c, rh_pct);
}
