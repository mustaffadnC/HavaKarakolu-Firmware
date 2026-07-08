#ifndef HK_SHT4X_H
#define HK_SHT4X_H

#include <stdbool.h>
#include <stdint.h>

#include "bus/i2c_bus_if.h"
#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sensirion SHT4x temperature/humidity sensor.
 *
 * Two units on the board share this code but live on different buses
 * (one on hardware I2C2, one on bit-banged I2C) because the address (0x44)
 * is fixed. Each instance binds an hk_i2c_bus_t.
 *
 * Measurement is split into trigger + fetch so an RTOS task can yield during
 * the ~9 ms conversion instead of busy-waiting:
 *     hk_sht4x_trigger(dev);
 *     vTaskDelay(pdMS_TO_TICKS(10));
 *     hk_sht4x_fetch(dev, &t, &rh);
 */

/* Measurement commands (precision). */
#define HK_SHT4X_CMD_MEAS_HIGH   0xFD
#define HK_SHT4X_CMD_MEAS_MED    0xF6
#define HK_SHT4X_CMD_MEAS_LOW    0xE0
#define HK_SHT4X_CMD_SOFT_RESET  0x94
#define HK_SHT4X_CMD_READ_SERIAL 0x89

typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;     /* 7-bit, normally 0x44 */
    uint8_t             meas_cmd; /* last triggered command */
    const char         *name;     /* for logs, e.g. "SHT4x_1" */
} hk_sht4x_t;

hk_status_t hk_sht4x_init(hk_sht4x_t *dev, const hk_i2c_bus_t *bus,
                          uint8_t addr7, const char *name);

hk_status_t hk_sht4x_soft_reset(hk_sht4x_t *dev);
hk_status_t hk_sht4x_read_serial(hk_sht4x_t *dev, uint32_t *serial);

/* Issue a measurement command (default high precision if cmd==0). */
hk_status_t hk_sht4x_trigger(hk_sht4x_t *dev, uint8_t cmd);
/* Read the 6-byte result and convert (call after the conversion delay). */
hk_status_t hk_sht4x_fetch(hk_sht4x_t *dev, float *temp_c, float *rh_pct);

/*
 * Pure parse of a 6-byte SHT4x response (T_msb,T_lsb,T_crc,RH_msb,RH_lsb,RH_crc)
 * into temperature [°C] and relative humidity [%], with CRC-8 verification.
 * Host-testable; no I/O. Returns HK_OK or HK_ERR_CRC / HK_ERR_PARAM.
 */
hk_status_t hk_sht4x_parse(const uint8_t raw[6], float *temp_c, float *rh_pct);

#ifdef __cplusplus
}
#endif

#endif /* HK_SHT4X_H */
