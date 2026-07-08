#include "services/sensor_manager/sensor_manager.h"

#if !defined(HK_HOST)

#include "bsp/board_config.h"
#include "common/hk_time.h"
#include "common/log.h"
#include "services/system_state/system_state.h"

hk_status_t hk_sensors_init(hk_sensor_mgr_t *m,
                            const hk_i2c_bus_t *i2c_main,
                            const hk_i2c_bus_t *i2c_sht1,
                            const hk_i2c_bus_t *i2c_sht2)
{
    if (m == NULL) {
        return HK_ERR_PARAM;
    }
    m->baro_ref_pa = HK_STD_PRESSURE_PA;
    hk_comp_filter_init(&m->attitude, 0.98f);

    hk_status_t bmp = hk_bmp280_init(&m->bmp, i2c_main, HK_ADDR_BMP280);
    hk_state_set_sensor_ok(HK_SENSOR_BARO, bmp == HK_OK);

    (void)hk_sht4x_init(&m->sht1, i2c_sht1, HK_ADDR_SHT4X, "SHT1");
    (void)hk_sht4x_init(&m->sht2, i2c_sht2, HK_ADDR_SHT4X, "SHT2");
    /* presence confirmed at first successful read */

    hk_status_t imu = hk_bmi270_init(&m->imu, i2c_main, HK_ADDR_BMI270);
    hk_state_set_sensor_ok(HK_SENSOR_BMI270, imu == HK_OK);

    HK_LOGI("sensors", "bmp=%s imu=%s", hk_status_str(bmp), hk_status_str(imu));
    return HK_OK;
}

void hk_sensors_set_baro_ref(hk_sensor_mgr_t *m)
{
    float t, p;
    if (hk_bmp280_read(&m->bmp, &t, &p) == HK_OK && p > 0.0f) {
        m->baro_ref_pa = p;
        HK_LOGI("sensors", "baro ref = %.0f Pa", (double)p);
    }
}

void hk_sensors_sample_env(hk_sensor_mgr_t *m)
{
    float t_bmp = 0, p = 0, alt = 0;
    bool  bmp_ok = (hk_bmp280_read_altitude(&m->bmp, m->baro_ref_pa,
                                            &t_bmp, &p, &alt) == HK_OK);

    /* SHT4x: trigger both, wait once, fetch both. */
    bool s1_trig = (hk_sht4x_trigger(&m->sht1, 0) == HK_OK);
    bool s2_trig = (hk_sht4x_trigger(&m->sht2, 0) == HK_OK);
    hk_delay_ms(10);

    float t1 = 0, h1 = 0, t2 = 0, h2 = 0;
    bool  s1_ok = s1_trig && (hk_sht4x_fetch(&m->sht1, &t1, &h1) == HK_OK);
    bool  s2_ok = s2_trig && (hk_sht4x_fetch(&m->sht2, &t2, &h2) == HK_OK);

    hk_system_state_t *s = hk_state_lock();
    if (bmp_ok) { s->temp_bmp_c = t_bmp; s->pressure_pa = p; s->altitude_m = alt; }
    if (s1_ok)  { s->temp_sht1_c = t1; s->rh_sht1 = h1; }
    if (s2_ok)  { s->temp_sht2_c = t2; s->rh_sht2 = h2; }
    hk_state_unlock();

    hk_state_set_sensor_ok(HK_SENSOR_BARO, bmp_ok);
    hk_state_set_sensor_ok(HK_SENSOR_SHT1, s1_ok);
    hk_state_set_sensor_ok(HK_SENSOR_SHT2, s2_ok);
}

void hk_sensors_sample_imu(hk_sensor_mgr_t *m, float dt)
{
    hk_vec3f a = {0}, g = {0};
    if (hk_bmi270_read(&m->imu, &a, &g) != HK_OK) {
        hk_state_set_sensor_ok(HK_SENSOR_BMI270, false);
        return;
    }
    hk_comp_filter_update(&m->attitude, &a, &g, dt);

    hk_system_state_t *s = hk_state_lock();
    s->accel     = a;
    s->gyro      = g;
    s->roll_deg  = m->attitude.roll_deg;
    s->pitch_deg = m->attitude.pitch_deg;
    hk_state_unlock();
    hk_state_set_sensor_ok(HK_SENSOR_BMI270, true);
}

#endif /* !HK_HOST */
