#include "drivers/bmp581/bmp581.h"
#include "hk_test.h"
#include "mocks/mock_i2c.h"

#include <math.h>
#include <string.h>

#include "common/units.h"

/* ---- hk_time shims (driver polls with delays; instant on host) ---- */
void     hk_time_init(void) {}
uint32_t hk_millis(void) { return 0; }
void     hk_delay_ms(uint32_t ms) { (void)ms; }
void     hk_delay_us(uint32_t us) { (void)us; }

/* Register traffic, per BST-BMP581-DS004 §7. */
static const uint8_t w_reset[]     = { 0x7E, 0xB6 };
static const uint8_t w_id[]        = { 0x01 };
static const uint8_t w_standby[]   = { 0x37, 0x80 };  /* deep_dis | STANDBY */
static const uint8_t w_osr[]       = { 0x36, 0x5B };  /* press_en, x8 / x8  */
static const uint8_t w_osr_rd[]    = { 0x36 };
static const uint8_t w_forced[]    = { 0x37, 0x82 };  /* deep_dis | FORCED  */
static const uint8_t w_odr_rd[]    = { 0x37 };
static const uint8_t w_data[]      = { 0x1D };

static const uint8_t r_id_ok[]     = { 0x50 };
static const uint8_t r_id_other[]  = { 0x60 };        /* e.g. a BMP39x      */
static const uint8_t r_osr_ok[]    = { 0x5B };
static const uint8_t r_osr_lost[]  = { 0x00 };        /* write swallowed    */
static const uint8_t r_busy[]      = { 0x82 };        /* still FORCED       */
static const uint8_t r_done[]      = { 0x80 };        /* back in STANDBY    */

/* T = raw/2^16 -> 0x190000 = 25.0 C;  p = raw/2^6 -> 0x62F340 = 101325 Pa.
 * Both little-endian in the 0x1D..0x22 burst. */
static const uint8_t k_raw6[6] = { 0x00, 0x00, 0x19, 0x40, 0xF3, 0x62 };

/* -10.0 C = -655360 -> 24-bit two's complement 0xF60000. */
static const uint8_t k_raw6_cold[6] = { 0x00, 0x00, 0xF6, 0x40, 0xF3, 0x62 };

#define INIT_STEPS(addr)                                    \
    { (addr), w_reset,   2, NULL,      0, HK_OK },          \
    { (addr), w_id,      1, r_id_ok,   1, HK_OK },          \
    { (addr), w_standby, 2, NULL,      0, HK_OK },          \
    { (addr), w_osr,     2, NULL,      0, HK_OK },          \
    { (addr), w_osr_rd,  1, r_osr_ok,  1, HK_OK }

static void test_full_read_flow(void)
{
    const hk_mock_i2c_step_t script[] = {
        INIT_STEPS(0x46),
        /* one forced conversion: request, poll busy, poll done, burst read */
        { 0x46, w_forced, 2, NULL,   0, HK_OK },
        { 0x46, w_odr_rd, 1, r_busy, 1, HK_OK },
        { 0x46, w_odr_rd, 1, r_done, 1, HK_OK },
        { 0x46, w_data,   1, k_raw6, 6, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp581_t dev;
    HK_CHECK_EQ_INT(hk_bmp581_init(&dev, &bus, 0), HK_OK);
    HK_CHECK_EQ_INT(dev.addr, 0x46);          /* default = SDO grounded */

    float t = 0, p = 0;
    HK_CHECK_EQ_INT(hk_bmp581_read(&dev, &t, &p), HK_OK);
    HK_CHECK(fabsf(t - 25.0f) < 0.001f);
    HK_CHECK(fabsf(p - 101325.0f) < 0.01f);

    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_negative_temperature_sign_extends(void)
{
    const hk_mock_i2c_step_t script[] = {
        INIT_STEPS(0x47),
        { 0x47, w_forced, 2, NULL,         0, HK_OK },
        { 0x47, w_odr_rd, 1, r_done,       1, HK_OK },
        { 0x47, w_data,   1, k_raw6_cold,  6, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp581_t dev;
    HK_CHECK_EQ_INT(hk_bmp581_init(&dev, &bus, 0x47), HK_OK);

    float t = 0, p = 0;
    HK_CHECK_EQ_INT(hk_bmp581_read(&dev, &t, &p), HK_OK);
    HK_CHECK(fabsf(t + 10.0f) < 0.001f);      /* -10.0 C, not +246 C */
    HK_CHECK(fabsf(p - 101325.0f) < 0.01f);

    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_rejects_foreign_chip_id(void)
{
    const hk_mock_i2c_step_t script[] = {
        { 0x46, w_reset, 2, NULL,        0, HK_OK },
        { 0x46, w_id,    1, r_id_other,  1, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp581_t dev;
    HK_CHECK_EQ_INT(hk_bmp581_init(&dev, &bus, 0), HK_ERR_NOT_FOUND);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

/* Regression: the part boots into DEEP STANDBY, where configuration writes
 * are dropped. Init must notice via the read-back instead of running the
 * whole flight at 1x oversampling. */
static void test_detects_config_lost_in_deep_standby(void)
{
    const hk_mock_i2c_step_t script[] = {
        { 0x46, w_reset,   2, NULL,        0, HK_OK },
        { 0x46, w_id,      1, r_id_ok,     1, HK_OK },
        { 0x46, w_standby, 2, NULL,        0, HK_OK },
        { 0x46, w_osr,     2, NULL,        0, HK_OK },
        { 0x46, w_osr_rd,  1, r_osr_lost,  1, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp581_t dev;
    HK_CHECK_EQ_INT(hk_bmp581_init(&dev, &bus, 0), HK_ERR_IO);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_conversion_timeout(void)
{
    /* Request + 8 polls that never leave FORCED. */
    hk_mock_i2c_step_t script[9];
    script[0] = (hk_mock_i2c_step_t){ 0x46, w_forced, 2, NULL, 0, HK_OK };
    for (size_t i = 1; i < 9; ++i) {
        script[i] = (hk_mock_i2c_step_t){ 0x46, w_odr_rd, 1, r_busy, 1, HK_OK };
    }
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, 9);

    hk_bmp581_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.bus  = &bus;
    dev.addr = 0x46;

    float t = 0, p = 0;
    HK_CHECK_EQ_INT(hk_bmp581_read(&dev, &t, &p), HK_ERR_TIMEOUT);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_bus_error_propagates(void)
{
    const hk_mock_i2c_step_t script[] = {
        { 0x46, w_reset, 2, NULL, 0, HK_ERR_IO },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp581_t dev;
    HK_CHECK_EQ_INT(hk_bmp581_init(&dev, &bus, 0), HK_ERR_IO);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_altitude_sanity(void)
{
    /* p == ref -> 0 m; 95000 Pa vs ISA sea level -> ~540 m. */
    HK_CHECK(fabsf(hk_altitude_from_pressure(101325.0f, 101325.0f)) < 0.01f);
    float alt = hk_altitude_from_pressure(95000.0f, 101325.0f);
    HK_CHECK(alt > 530.0f && alt < 555.0f);
}

static void test_param_guards(void)
{
    hk_bmp581_t dev;
    float t, p;
    HK_CHECK_EQ_INT(hk_bmp581_init(NULL, NULL, 0), HK_ERR_PARAM);
    HK_CHECK_EQ_INT(hk_bmp581_read(NULL, &t, &p), HK_ERR_PARAM);
    memset(&dev, 0, sizeof(dev));
    HK_CHECK_EQ_INT(hk_bmp581_read(&dev, NULL, &p), HK_ERR_PARAM);
    HK_CHECK_EQ_INT(hk_bmp581_read(&dev, &t, NULL), HK_ERR_PARAM);
}

int main(void)
{
    printf("test_bmp581\n");
    test_full_read_flow();
    test_negative_temperature_sign_extends();
    test_rejects_foreign_chip_id();
    test_detects_config_lost_in_deep_standby();
    test_conversion_timeout();
    test_bus_error_propagates();
    test_altitude_sanity();
    test_param_guards();
    return hk_test_summary();
}
