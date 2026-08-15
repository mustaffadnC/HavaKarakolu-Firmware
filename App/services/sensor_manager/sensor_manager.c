#include "services/sensor_manager/sensor_manager.h"

#include <stddef.h>

#if !defined(HK_HOST)

#include "bsp/board_config.h"
#include "common/hk_time.h"
#include "common/log.h"
#include "services/system_state/system_state.h"

/* Wrap-safe "may this device be probed again?" -- the signed difference keeps
 * working across the 49-day hk_millis() rollover. */
static bool retry_due(uint32_t now, uint32_t at_ms)
{
    return (int32_t)(now - at_ms) >= 0;
}

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
    hk_deriv_lpf_init(&m->vspeed, 0.3f);

    hk_status_t bmp = hk_bmp581_init(&m->bmp, i2c_main, HK_ADDR_BMP581);
    hk_status_t imu = hk_bmi270_init(&m->imu, i2c_main, HK_ADDR_BMI270);

    /* Every device on this bus silent at once usually means the bus itself is
     * stuck, not that the parts are missing: a slave interrupted mid-byte
     * keeps SDA pulled down forever and the master then sees an idle-looking
     * line it can never drive. The standard escape is to clock SCL until the
     * slave finishes its byte and releases SDA, so do that once and retry
     * before believing the sensors are absent. Observed on the first board,
     * 2026-08-01: SDA on I2C1 low at boot with the arbitration-lost flag set. */
    if (bmp != HK_OK && imu != HK_OK && i2c_main->recover != NULL) {
        HK_LOGW("sensors", "i2c_main silent; clearing bus and retrying");
        i2c_main->recover(i2c_main->ctx);
        bmp = hk_bmp581_init(&m->bmp, i2c_main, HK_ADDR_BMP581);
        imu = hk_bmi270_init(&m->imu, i2c_main, HK_ADDR_BMI270);
    }

    hk_state_set_sensor_ok(HK_SENSOR_BARO, bmp == HK_OK);
    hk_state_set_sensor_ok(HK_SENSOR_BMI270, imu == HK_OK);

    (void)hk_sht4x_init(&m->sht1, i2c_sht1, HK_ADDR_SHT4X, "SHT1");
    (void)hk_sht4x_init(&m->sht2, i2c_sht2, HK_ADDR_SHT4X, "SHT2");
    /* presence confirmed at first successful read */

    /* A part that already failed to answer here goes straight into backoff, so
     * the sampling tasks never pay its bus timeout on their very first pass. */
    uint32_t now = hk_millis();
    m->bmp_retry_at_ms  = (bmp == HK_OK) ? now : now + HK_SENSOR_RETRY_MS;
    m->imu_retry_at_ms  = (imu == HK_OK) ? now : now + HK_SENSOR_RETRY_MS;
    m->sht1_retry_at_ms = now;
    m->sht2_retry_at_ms = now;

    HK_LOGI("sensors", "bmp=%s imu=%s", hk_status_str(bmp), hk_status_str(imu));
    return HK_OK;
}

void hk_sensors_set_baro_ref(hk_sensor_mgr_t *m)
{
    float t, p;
    if (hk_bmp581_read(&m->bmp, &t, &p) == HK_OK && p > 0.0f) {
        m->baro_ref_pa = p;
        HK_LOGI("sensors", "baro ref = %.0f Pa", (double)p);
    }
}

void hk_sensors_sample_env(hk_sensor_mgr_t *m, float dt_s)
{
    uint32_t now = hk_millis();

    float t_bmp = 0, p = 0, alt = 0;
    bool  bmp_try = retry_due(now, m->bmp_retry_at_ms);
    bool  bmp_ok  = bmp_try && (hk_bmp581_read_altitude(&m->bmp, m->baro_ref_pa,
                                                        &t_bmp, &p, &alt) == HK_OK);
    if (bmp_try) {
        m->bmp_retry_at_ms = bmp_ok ? now : now + HK_SENSOR_RETRY_MS;
    }

    float vs = bmp_ok ? hk_deriv_lpf_update(&m->vspeed, alt, dt_s)
                      : m->vspeed.value;

    /* SHT4x: trigger both, wait once, fetch both. */
    bool s1_try  = retry_due(now, m->sht1_retry_at_ms);
    bool s2_try  = retry_due(now, m->sht2_retry_at_ms);
    bool s1_trig = s1_try && (hk_sht4x_trigger(&m->sht1, 0) == HK_OK);
    bool s2_trig = s2_try && (hk_sht4x_trigger(&m->sht2, 0) == HK_OK);
    if (s1_trig || s2_trig) {
        hk_delay_ms(10);   /* conversion time; pointless if neither answered */
    }

    float t1 = 0, h1 = 0, t2 = 0, h2 = 0;
    bool  s1_ok = s1_trig && (hk_sht4x_fetch(&m->sht1, &t1, &h1) == HK_OK);
    bool  s2_ok = s2_trig && (hk_sht4x_fetch(&m->sht2, &t2, &h2) == HK_OK);
    if (s1_try) { m->sht1_retry_at_ms = s1_ok ? now : now + HK_SENSOR_RETRY_MS; }
    if (s2_try) { m->sht2_retry_at_ms = s2_ok ? now : now + HK_SENSOR_RETRY_MS; }

    hk_system_state_t *s = hk_state_lock();
    if (bmp_ok) { s->temp_bmp_c = t_bmp; s->pressure_pa = p; s->altitude_m = alt;
                  s->vspeed_ms = vs; }
    if (s1_ok)  { s->temp_sht1_c = t1; s->rh_sht1 = h1; }
    if (s2_ok)  { s->temp_sht2_c = t2; s->rh_sht2 = h2; }
    hk_state_unlock();

    /* Only report on devices actually probed this pass; a skipped one keeps the
     * flag its last real attempt produced instead of flapping to false. */
    if (bmp_try) { hk_state_set_sensor_ok(HK_SENSOR_BARO, bmp_ok); }
    if (s1_try)  { hk_state_set_sensor_ok(HK_SENSOR_SHT1, s1_ok); }
    if (s2_try)  { hk_state_set_sensor_ok(HK_SENSOR_SHT2, s2_ok); }
}

void hk_sensors_sample_imu(hk_sensor_mgr_t *m, float dt)
{
    uint32_t now = hk_millis();
    if (!retry_due(now, m->imu_retry_at_ms)) {
        return;   /* known silent: stay off the bus so the 100 Hz period holds */
    }

    hk_vec3f a = {0}, g = {0};
    if (hk_bmi270_read(&m->imu, &a, &g) != HK_OK) {
        m->imu_retry_at_ms = now + HK_SENSOR_RETRY_MS;
        hk_state_set_sensor_ok(HK_SENSOR_BMI270, false);
        return;
    }
    m->imu_retry_at_ms = now;   /* healthy: sample again next cycle */
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
