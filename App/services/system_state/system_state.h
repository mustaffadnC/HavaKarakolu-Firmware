#ifndef HK_SYSTEM_STATE_H
#define HK_SYSTEM_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "common/units.h"
#include "drivers/gps_ublox/nmea.h"
#include "services/mission/mission.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sensor presence/health bit flags (system_state.sensor_ok). */
typedef enum {
    HK_SENSOR_BARO   = 1u << 0,   /* BMP280 on rev-2 boards */
    HK_SENSOR_SHT1   = 1u << 1,
    HK_SENSOR_SHT2   = 1u << 2,
    HK_SENSOR_BMI270 = 1u << 3,
    HK_SENSOR_GPS    = 1u << 4,
    HK_SENSOR_BATT   = 1u << 5,
    HK_SENSOR_SD     = 1u << 6    /* SD card mounted and writable */
} hk_sensor_flag_t;

/* Central, mutex-protected snapshot of everything the system knows. */
typedef struct {
    /* environment */
    float temp_bmp_c;
    float temp_sht1_c, rh_sht1;
    float temp_sht2_c, rh_sht2;
    float pressure_pa;
    float altitude_m;       /* baro, relative to baro_ref */
    float vspeed_ms;        /* filtered vertical speed, + = up */

    /* inertial */
    hk_vec3f accel;         /* m/s^2 */
    hk_vec3f gyro;          /* rad/s */
    float    roll_deg, pitch_deg;

    /* navigation */
    hk_gps_fix_t gps;

    /* power */
    float batt_voltage;
    float batt_soc;

    /* mission / health */
    hk_mission_state_t mission;
    uint8_t            buzzer_pattern;  /* hk_buzzer_pattern_t (mission out) */
    uint32_t           sensor_ok;   /* bitmask of hk_sensor_flag_t */
    uint32_t           uptime_ms;
} hk_system_state_t;

void               hk_state_init(void);

/* Snapshot for readers (telemetry, mission): locks, copies, unlocks. */
void               hk_state_get(hk_system_state_t *out);

/* Direct access for writers: lock, mutate the returned pointer, then unlock. */
hk_system_state_t *hk_state_lock(void);
void               hk_state_unlock(void);

/* Helpers to flip a sensor health flag (locks internally). */
void               hk_state_set_sensor_ok(hk_sensor_flag_t flag, bool ok);

#ifdef __cplusplus
}
#endif

#endif /* HK_SYSTEM_STATE_H */
