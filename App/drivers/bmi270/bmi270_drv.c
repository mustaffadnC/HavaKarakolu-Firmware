#include "drivers/bmi270/bmi270_drv.h"

#if defined(HK_USE_BMI270)

#include <string.h>

#include "common/hk_time.h"
#include "bmi270.h"   /* vendored Bosch SensorAPI (pulls in bmi2.h, bmi2_defs.h) */

/* Single-instance device context bridged to our bus abstraction. */
typedef struct {
    const hk_i2c_bus_t *bus;
    uint8_t             addr;
} bmi2_intf_t;

static bmi2_intf_t    s_intf;
static struct bmi2_dev s_dev;

static BMI2_INTF_RETURN_TYPE i2c_read(uint8_t reg, uint8_t *data,
                                      uint32_t len, void *intf_ptr)
{
    bmi2_intf_t *w = (bmi2_intf_t *)intf_ptr;
    return (hk_i2c_read_reg(w->bus, w->addr, reg, data, len) == HK_OK)
               ? BMI2_INTF_RET_SUCCESS : -1;
}

static BMI2_INTF_RETURN_TYPE i2c_write(uint8_t reg, const uint8_t *data,
                                       uint32_t len, void *intf_ptr)
{
    bmi2_intf_t *w = (bmi2_intf_t *)intf_ptr;
    uint8_t buf[64];
    if (len > sizeof(buf) - 1U) {
        return -1;
    }
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return (hk_i2c_write(w->bus, w->addr, buf, len + 1U) == HK_OK)
               ? BMI2_INTF_RET_SUCCESS : -1;
}

static void delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    hk_delay_us(period);
}

hk_status_t hk_bmi270_init(hk_bmi270_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7)
{
    if (dev == NULL || bus == NULL) {
        return HK_ERR_PARAM;
    }
    dev->bus  = bus;
    dev->addr = (addr7 == 0) ? 0x68 : addr7;

    s_intf.bus  = dev->bus;
    s_intf.addr = dev->addr;

    s_dev.intf          = BMI2_I2C_INTF;
    s_dev.read          = i2c_read;
    s_dev.write         = i2c_write;
    s_dev.delay_us      = delay_us;
    s_dev.intf_ptr      = &s_intf;
    s_dev.read_write_len = 32;   /* config blob is chunked to <= this */
    s_dev.config_file_ptr = NULL; /* use default bmi270 config */

    if (bmi270_init(&s_dev) != BMI2_OK) {
        return HK_ERR_NOT_FOUND;
    }

    struct bmi2_sens_config cfg[2];
    cfg[0].type = BMI2_ACCEL;
    cfg[1].type = BMI2_GYRO;
    if (bmi2_get_sensor_config(cfg, 2, &s_dev) != BMI2_OK) {
        return HK_ERR;
    }

    cfg[0].cfg.acc.odr         = BMI2_ACC_ODR_100HZ;
    cfg[0].cfg.acc.range       = BMI2_ACC_RANGE_8G;
    cfg[0].cfg.acc.bwp         = BMI2_ACC_NORMAL_AVG4;
    cfg[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

    cfg[1].cfg.gyr.odr         = BMI2_GYR_ODR_100HZ;
    cfg[1].cfg.gyr.range       = BMI2_GYR_RANGE_2000;
    cfg[1].cfg.gyr.bwp         = BMI2_GYR_NORMAL_MODE;
    cfg[1].cfg.gyr.noise_perf  = BMI2_PERF_OPT_MODE;
    cfg[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

    if (bmi2_set_sensor_config(cfg, 2, &s_dev) != BMI2_OK) {
        return HK_ERR;
    }

    uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_GYRO };
    if (bmi2_sensor_enable(sens_list, 2, &s_dev) != BMI2_OK) {
        return HK_ERR;
    }

    /* Scale factors: full-scale / 2^15, then to SI. */
    dev->acc_lsb_ms2 = (8.0f / 32768.0f) * HK_GRAVITY_MS2;
    dev->gyr_lsb_rps = (2000.0f / 32768.0f) * HK_DEG2RAD;
    return HK_OK;
}

hk_status_t hk_bmi270_read(hk_bmi270_t *dev, hk_vec3f *accel, hk_vec3f *gyro)
{
    if (dev == NULL) {
        return HK_ERR_PARAM;
    }
    struct bmi2_sens_data data;
    memset(&data, 0, sizeof(data));
    if (bmi2_get_sensor_data(&data, &s_dev) != BMI2_OK) {
        return HK_ERR_IO;
    }
    if (accel != NULL) {
        accel->x = (float)data.acc.x * dev->acc_lsb_ms2;
        accel->y = (float)data.acc.y * dev->acc_lsb_ms2;
        accel->z = (float)data.acc.z * dev->acc_lsb_ms2;
    }
    if (gyro != NULL) {
        gyro->x = (float)data.gyr.x * dev->gyr_lsb_rps;
        gyro->y = (float)data.gyr.y * dev->gyr_lsb_rps;
        gyro->z = (float)data.gyr.z * dev->gyr_lsb_rps;
    }
    return HK_OK;
}

#else /* !HK_USE_BMI270 : stubs so the firmware builds without the vendor lib */

hk_status_t hk_bmi270_init(hk_bmi270_t *dev, const hk_i2c_bus_t *bus, uint8_t addr7)
{
    (void)dev; (void)bus; (void)addr7;
    return HK_ERR_NOT_FOUND;
}

hk_status_t hk_bmi270_read(hk_bmi270_t *dev, hk_vec3f *accel, hk_vec3f *gyro)
{
    (void)dev; (void)accel; (void)gyro;
    return HK_ERR_NOT_FOUND;
}

#endif /* HK_USE_BMI270 */
