#include "drivers/bmp280/bmp280.h"
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

/* Datasheet §3.12 example calibration + raw readings. */
static const hk_bmp280_calib_t k_calib = {
    .dig_t1 = 27504, .dig_t2 = 26435,  .dig_t3 = -1000,
    .dig_p1 = 36477, .dig_p2 = -10685, .dig_p3 = 3024,
    .dig_p4 = 2855,  .dig_p5 = 140,    .dig_p6 = -7,
    .dig_p7 = 15500, .dig_p8 = -14600, .dig_p9 = 6000,
};
#define ADC_T 519888
#define ADC_P 415148

/* Same coefficients as the little-endian NVM blob at 0x88..0x9F. */
static const uint8_t k_calib_blob[24] = {
    0x70, 0x6B,   /* T1 = 27504  */
    0x43, 0x67,   /* T2 = 26435  */
    0x18, 0xFC,   /* T3 = -1000  */
    0x7D, 0x8E,   /* P1 = 36477  */
    0x43, 0xD6,   /* P2 = -10685 */
    0xD0, 0x0B,   /* P3 = 3024   */
    0x27, 0x0B,   /* P4 = 2855   */
    0x8C, 0x00,   /* P5 = 140    */
    0xF9, 0xFF,   /* P6 = -7     */
    0x8C, 0x3C,   /* P7 = 15500  */
    0xF8, 0xC6,   /* P8 = -14600 */
    0x70, 0x17,   /* P9 = 6000   */
};

/* Raw data registers 0xF7..0xFC for ADC_P / ADC_T (msb, lsb, xlsb<<4). */
static const uint8_t k_raw6[6] = { 0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x00 };

static const uint8_t w_reset[]  = { 0xE0, 0xB6 };
static const uint8_t w_id[]     = { 0xD0 };
static const uint8_t w_calib[]  = { 0x88 };
static const uint8_t w_config[] = { 0xF5, 0x08 };
static const uint8_t w_trig[]   = { 0xF4, 0x55 };
static const uint8_t w_status[] = { 0xF3 };
static const uint8_t w_data[]   = { 0xF7 };

static const uint8_t r_id_ok[]     = { 0x58 };
static const uint8_t r_id_bme[]    = { 0x60 };
static const uint8_t r_idle[]      = { 0x00 };
static const uint8_t r_measuring[] = { 0x08 };

static void test_compensation_datasheet_exact(void)
{
    int32_t t_fine = 0;
    int32_t t = hk_bmp280_comp_temp(&k_calib, ADC_T, &t_fine);
    HK_CHECK_EQ_INT(t, 2508);          /* 25.08 degC */
    HK_CHECK_EQ_INT(t_fine, 128422);

    uint32_t p = hk_bmp280_comp_press(&k_calib, ADC_P, t_fine);
    HK_CHECK_EQ_INT((long)p, 25767233L);   /* 100653.254 Pa in Q24.8 */
}

static void test_calib_parse(void)
{
    hk_bmp280_calib_t c;
    memset(&c, 0, sizeof(c));
    hk_bmp280_parse_calib(k_calib_blob, &c);
    HK_CHECK_EQ_INT(c.dig_t1, k_calib.dig_t1);
    HK_CHECK_EQ_INT(c.dig_t2, k_calib.dig_t2);
    HK_CHECK_EQ_INT(c.dig_t3, k_calib.dig_t3);
    HK_CHECK_EQ_INT(c.dig_p1, k_calib.dig_p1);
    HK_CHECK_EQ_INT(c.dig_p2, k_calib.dig_p2);
    HK_CHECK_EQ_INT(c.dig_p3, k_calib.dig_p3);
    HK_CHECK_EQ_INT(c.dig_p4, k_calib.dig_p4);
    HK_CHECK_EQ_INT(c.dig_p5, k_calib.dig_p5);
    HK_CHECK_EQ_INT(c.dig_p6, k_calib.dig_p6);
    HK_CHECK_EQ_INT(c.dig_p7, k_calib.dig_p7);
    HK_CHECK_EQ_INT(c.dig_p8, k_calib.dig_p8);
    HK_CHECK_EQ_INT(c.dig_p9, k_calib.dig_p9);
}

static void test_full_read_flow(void)
{
    const hk_mock_i2c_step_t script[] = {
        { 0x76, w_reset,  2, NULL,         0,  HK_OK },
        { 0x76, w_id,     1, r_id_ok,      1,  HK_OK },
        { 0x76, w_calib,  1, k_calib_blob, 24, HK_OK },
        { 0x76, w_config, 2, NULL,         0,  HK_OK },
        { 0x76, w_trig,   2, NULL,         0,  HK_OK },
        { 0x76, w_status, 1, r_idle,       1,  HK_OK },
        { 0x76, w_data,   1, k_raw6,       6,  HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp280_t dev;
    HK_CHECK_EQ_INT(hk_bmp280_init(&dev, &bus, 0), HK_OK);
    HK_CHECK_EQ_INT(dev.addr, 0x76);

    float t = 0, p = 0;
    HK_CHECK_EQ_INT(hk_bmp280_read(&dev, &t, &p), HK_OK);
    HK_CHECK(fabsf(t - 25.08f) < 0.005f);
    HK_CHECK(fabsf(p - 100653.254f) < 0.01f);

    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_rejects_bme280(void)
{
    const hk_mock_i2c_step_t script[] = {
        { 0x76, w_reset, 2, NULL,     0, HK_OK },
        { 0x76, w_id,    1, r_id_bme, 1, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp280_t dev;
    HK_CHECK_EQ_INT(hk_bmp280_init(&dev, &bus, 0), HK_ERR_NOT_FOUND);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_rejects_blank_calibration(void)
{
    static const uint8_t blank[24] = { 0 };
    const hk_mock_i2c_step_t script[] = {
        { 0x76, w_reset, 2, NULL,    0,  HK_OK },
        { 0x76, w_id,    1, r_id_ok, 1,  HK_OK },
        { 0x76, w_calib, 1, blank,   24, HK_OK },
    };
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, sizeof(script) / sizeof(script[0]));

    hk_bmp280_t dev;
    HK_CHECK_EQ_INT(hk_bmp280_init(&dev, &bus, 0), HK_ERR_CRC);
    HK_CHECK(hk_mock_i2c_done(&mock));
}

static void test_conversion_timeout(void)
{
    /* Trigger + 26 status polls (waited 10,12,...,60 ms) all "measuring". */
    hk_mock_i2c_step_t script[27];
    script[0] = (hk_mock_i2c_step_t){ 0x76, w_trig, 2, NULL, 0, HK_OK };
    for (size_t i = 1; i < 27; ++i) {
        script[i] = (hk_mock_i2c_step_t){ 0x76, w_status, 1, r_measuring, 1, HK_OK };
    }
    hk_mock_i2c_t mock;
    hk_i2c_bus_t  bus;
    hk_mock_i2c_init(&mock, &bus, script, 27);

    hk_bmp280_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.bus   = &bus;
    dev.addr  = 0x76;
    dev.calib = k_calib;

    float t = 0, p = 0;
    HK_CHECK_EQ_INT(hk_bmp280_read(&dev, &t, &p), HK_ERR_TIMEOUT);
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
    hk_bmp280_t dev;
    float t, p;
    HK_CHECK_EQ_INT(hk_bmp280_init(NULL, NULL, 0), HK_ERR_PARAM);
    HK_CHECK_EQ_INT(hk_bmp280_read(NULL, &t, &p), HK_ERR_PARAM);
    memset(&dev, 0, sizeof(dev));
    HK_CHECK_EQ_INT(hk_bmp280_read(&dev, NULL, &p), HK_ERR_PARAM);
    HK_CHECK_EQ_INT(hk_bmp280_read(&dev, &t, NULL), HK_ERR_PARAM);
}

int main(void)
{
    printf("test_bmp280\n");
    test_compensation_datasheet_exact();
    test_calib_parse();
    test_full_read_flow();
    test_rejects_bme280();
    test_rejects_blank_calibration();
    test_conversion_timeout();
    test_altitude_sanity();
    test_param_guards();
    return hk_test_summary();
}
