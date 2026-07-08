/*
 * BMI270 wrapper + vendored Bosch SensorAPI, compiled for the host with a
 * register-map I2C mock. Proves that:
 *   - the vendor sources compile and link (config blob included)
 *   - the full init path (chip id, soft reset, config upload, INTERNAL_STATUS,
 *     accel/gyro config, sensor enable) succeeds against the wrapper
 *   - raw LSB -> SI conversion is correct at +/-8 g and +/-2000 dps
 */
#include "drivers/bmi270/bmi270_drv.h"
#include "hk_test.h"

#include <math.h>
#include <string.h>

/* ---- hk_time shims ---- */
void     hk_time_init(void) {}
uint32_t hk_millis(void) { return 0; }
void     hk_delay_ms(uint32_t ms) { (void)ms; }
void     hk_delay_us(uint32_t us) { (void)us; }

/* ---- register-map I2C mock (auto-increment bursts, INIT_DATA sink) ---- */

#define REG_CHIP_ID          0x00
#define REG_STATUS           0x03
#define REG_ACC_X_LSB        0x0C
#define REG_INTERNAL_STATUS  0x21
#define REG_INIT_DATA        0x5E

static uint8_t s_regs[256];
static uint32_t s_reads, s_writes;

static hk_status_t rm_write(void *ctx, uint8_t addr7,
                            const uint8_t *data, size_t len)
{
    (void)ctx; (void)addr7;
    if (len < 1) {
        return HK_ERR_PARAM;
    }
    uint8_t reg = data[0];
    if (reg != REG_INIT_DATA) {          /* config stream is write-only FIFO */
        for (size_t i = 1; i < len; ++i) {
            s_regs[(uint8_t)(reg + (i - 1u))] = data[i];
        }
    }
    s_writes++;
    return HK_OK;
}

static hk_status_t rm_read(void *ctx, uint8_t addr7, uint8_t *buf, size_t len)
{
    (void)ctx; (void)addr7; (void)buf; (void)len;
    return HK_ERR_IO;   /* BMI2 never uses address-less reads */
}

static hk_status_t rm_write_read(void *ctx, uint8_t addr7,
                                 const uint8_t *wbuf, size_t wlen,
                                 uint8_t *rbuf, size_t rlen)
{
    (void)ctx; (void)addr7;
    if (wlen != 1) {
        return HK_ERR_PARAM;
    }
    uint8_t reg = wbuf[0];
    for (size_t i = 0; i < rlen; ++i) {
        rbuf[i] = s_regs[(uint8_t)(reg + i)];
    }
    s_reads++;
    return HK_OK;
}

static hk_status_t rm_probe(void *ctx, uint8_t addr7)
{
    (void)ctx; (void)addr7;
    return HK_OK;
}

static void regmap_reset(void)
{
    memset(s_regs, 0, sizeof(s_regs));
    s_regs[REG_CHIP_ID]         = 0x24;   /* BMI270 */
    s_regs[REG_INTERNAL_STATUS] = 0x01;   /* config load OK */
    s_regs[REG_STATUS]          = 0xC0;   /* acc + gyr data ready */
    s_reads = s_writes = 0;
}

static void put_i16(uint8_t reg, int16_t v)
{
    s_regs[reg]     = (uint8_t)((uint16_t)v & 0xFFu);
    s_regs[reg + 1] = (uint8_t)((uint16_t)v >> 8);
}

int main(void)
{
    printf("test_bmi270\n");

    hk_i2c_bus_t bus = {
        .ctx = NULL,
        .write = rm_write, .read = rm_read,
        .write_read = rm_write_read, .probe = rm_probe, .recover = NULL,
    };

    regmap_reset();

    /* known raw sample: 1 g on Z at +/-8 g, 1000 dps on X at +/-2000 dps */
    put_i16(REG_ACC_X_LSB + 0, 0);
    put_i16(REG_ACC_X_LSB + 2, 0);
    put_i16(REG_ACC_X_LSB + 4, 4096);     /* 32768/8 = 4096 LSB/g   */
    put_i16(REG_ACC_X_LSB + 6, 16384);    /* 16384 * 2000/32768 = 1000 dps */
    put_i16(REG_ACC_X_LSB + 8, 0);
    put_i16(REG_ACC_X_LSB + 10, 0);

    hk_bmi270_t dev;
    HK_CHECK_EQ_INT(hk_bmi270_init(&dev, &bus, 0), HK_OK);
    HK_CHECK_EQ_INT(dev.addr, 0x68);
    /* the config upload really streamed data (thousands of writes) */
    HK_CHECK(s_writes > 100);

    hk_vec3f a, g;
    HK_CHECK_EQ_INT(hk_bmi270_read(&dev, &a, &g), HK_OK);
    HK_CHECK(fabsf(a.x - 0.0f) < 1e-3f);
    HK_CHECK(fabsf(a.y - 0.0f) < 1e-3f);
    HK_CHECK(fabsf(a.z - 9.80665f) < 1e-3f);
    HK_CHECK(fabsf(g.x - (1000.0f * 0.017453292f)) < 1e-3f);
    HK_CHECK(fabsf(g.y - 0.0f) < 1e-6f);
    HK_CHECK(fabsf(g.z - 0.0f) < 1e-6f);

    /* wrong chip id -> NOT_FOUND (surfaces a soldered-on different part) */
    regmap_reset();
    s_regs[REG_CHIP_ID] = 0x23;
    HK_CHECK_EQ_INT(hk_bmi270_init(&dev, &bus, 0), HK_ERR_NOT_FOUND);

    /* config upload failure (INTERNAL_STATUS != ok) -> init fails */
    regmap_reset();
    s_regs[REG_INTERNAL_STATUS] = 0x02;   /* init error */
    HK_CHECK(hk_bmi270_init(&dev, &bus, 0) != HK_OK);

    return hk_test_summary();
}
