#ifndef HK_MISSION_H
#define HK_MISSION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mission phases (see plan §6). The state machine logic is implemented in
 * mission.c (F6-F7); this header defines the enum + name so system_state and
 * telemetry can reference it now.
 *
 *  BOOT -> SELFTEST -> ATTACHED -> ARMED -> RELEASE -> DESCENT -> LANDED -> RECOVERY
 *                          ^___________(disarm)__________|
 */
typedef enum {
    HK_MISSION_BOOT = 0,
    HK_MISSION_SELFTEST,
    HK_MISSION_ATTACHED,
    HK_MISSION_ARMED,
    HK_MISSION_RELEASE,
    HK_MISSION_DESCENT,
    HK_MISSION_LANDED,
    HK_MISSION_RECOVERY,
    HK_MISSION_COUNT
} hk_mission_state_t;

static inline const char *hk_mission_state_str(hk_mission_state_t s)
{
    switch (s) {
    case HK_MISSION_BOOT:     return "BOOT";
    case HK_MISSION_SELFTEST: return "SELFTEST";
    case HK_MISSION_ATTACHED: return "ATTACHED";
    case HK_MISSION_ARMED:    return "ARMED";
    case HK_MISSION_RELEASE:  return "RELEASE";
    case HK_MISSION_DESCENT:  return "DESCENT";
    case HK_MISSION_LANDED:   return "LANDED";
    case HK_MISSION_RECOVERY: return "RECOVERY";
    default:                  return "?";
    }
}

/* ---- state machine (pure logic: no HAL, no RTOS, host-testable) ----
 *
 * The mission task builds hk_mission_in_t from system_state each tick and
 * applies hk_mission_out_t to the actuators. Every threshold is parametric
 * (P7 stores them in NV flash / SD CONFIG.INI) so field tuning needs no
 * reflash -- important while the SWD question (docs/ee-questions.md S1) is
 * open. No mission spec exists yet: defaults are reasonable assumptions,
 * to be reviewed with the team. */

typedef enum {
    HK_ARM_AUTO_ALT = 0,   /* arm when carried above arm_altitude_m       */
    HK_ARM_EXTERNAL = 1    /* arm only on an external command             */
} hk_arm_mode_t;

/* Buzzer pattern ids the control task maps to actual tone sequences. */
typedef enum {
    HK_BUZZ_OFF = 0,
    HK_BUZZ_BOOT,          /* short boot chirp                            */
    HK_BUZZ_ARMED,         /* periodic short beep: stay clear             */
    HK_BUZZ_DESCENT,       /* fast beeps during release/descent           */
    HK_BUZZ_LANDED,        /* slow double beep                            */
    HK_BUZZ_RECOVERY       /* loud SOS-style beacon                       */
} hk_buzzer_pattern_t;

/* Event args recorded with each transition (why it happened). */
enum {
    HK_MISSION_ARG_NORMAL           = 0,
    HK_MISSION_ARG_SELFTEST_TIMEOUT = 1,
    HK_MISSION_ARG_CMD              = 2,   /* external arm/release/disarm */
    HK_MISSION_ARG_AUTO_ALT         = 3,
    HK_MISSION_ARG_FREEFALL         = 4,
    HK_MISSION_ARG_VSPEED           = 5,
    HK_MISSION_ARG_TIMEOUT_FAILSAFE = 6
};

typedef struct {
    /* SELFTEST */
    uint32_t selftest_required_mask;  /* sensor_ok bits that must be set     */
    uint32_t selftest_timeout_ms;     /* proceed (degraded) after this       */
    /* ATTACHED -> ARMED */
    uint8_t  arm_mode;                /* hk_arm_mode_t                       */
    float    arm_altitude_m;          /* AUTO_ALT: altitude above launch     */
    uint32_t arm_hold_ms;             /* ...sustained this long              */
    /* ARMED -> RELEASE (separation detect; command always works) */
    float    release_freefall_g;      /* |a| below this (in g)...            */
    uint32_t release_hold_ms;         /* ...for this long                    */
    float    release_vspeed_ms;       /* OR vspeed below this (negative)     */
    /* RELEASE -> DESCENT */
    uint32_t release_actuation_ms;    /* actuator dwell before DESCENT       */
    /* DESCENT -> LANDED */
    float    landed_vspeed_ms;        /* |vspeed| below this                 */
    float    landed_accel_tol_g;      /* and | |a|-1g | below this (in g)    */
    uint32_t landed_hold_ms;
    uint32_t descent_timeout_ms;      /* failsafe -> LANDED                  */
    /* LANDED -> RECOVERY */
    uint32_t recovery_after_ms;
    /* servo positions (deg) */
    float    servo_hold_deg;
    float    servo_release_deg;
} hk_mission_cfg_t;

/* Sensible defaults; every field open to override via config service. */
hk_mission_cfg_t hk_mission_default_cfg(void);

typedef struct {
    uint32_t t_ms;
    float    alt_m;          /* baro altitude relative to launch [m]        */
    float    vspeed_ms;      /* filtered vertical speed [m/s], + = up       */
    float    accel_g;        /* |accel| in g units (1.0 at rest)            */
    bool     baro_ok;
    bool     imu_ok;
    uint32_t sensor_ok_mask; /* system_state sensor_ok bits                 */
    /* external commands (uplink/button/test console), edge-triggered */
    bool     arm_cmd, disarm_cmd, release_cmd;
} hk_mission_in_t;

typedef struct {
    hk_mission_state_t state;
    bool     lock_engaged;   /* solenoid: true = hold the capsule           */
    float    servo_deg;
    uint8_t  buzzer_pattern; /* hk_buzzer_pattern_t                         */
    bool     event;          /* a transition happened during this step      */
    hk_mission_state_t event_from;
    uint16_t event_arg;      /* HK_MISSION_ARG_*                            */
} hk_mission_out_t;

typedef struct {
    hk_mission_cfg_t   cfg;
    hk_mission_state_t state;
    uint32_t t_enter_ms;     /* when the current state was entered          */
    uint32_t cond_a_ms;      /* condition-sustain timers (0 = inactive)     */
    uint32_t cond_b_ms;
    bool     degraded;       /* selftest passed by timeout, not by sensors  */
} hk_mission_t;

void hk_mission_init(hk_mission_t *m, const hk_mission_cfg_t *cfg);

/* Advance the machine one tick (call at 10-50 ms period). Outputs are
 * level-based (safe to re-apply every tick); `event` flags transitions. */
void hk_mission_step(hk_mission_t *m, const hk_mission_in_t *in,
                     hk_mission_out_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HK_MISSION_H */
