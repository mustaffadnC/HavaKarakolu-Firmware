#include "services/mission/mission.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/*
 * Pure mission state machine. Design rules:
 *  - every threshold comes from hk_mission_cfg_t (field-tunable, no reflash)
 *  - every sensor-driven transition is DEBOUNCED: the condition must hold
 *    for a configured duration; a single noisy sample never releases
 *  - degenerate modes: works baro-only, IMU-only, or (commands + timeouts
 *    only) with both dead -- it degrades, it does not get stuck
 *  - outputs are levels, not pulses: safe to re-apply every tick
 */

hk_mission_cfg_t hk_mission_default_cfg(void)
{
    hk_mission_cfg_t c;
    memset(&c, 0, sizeof(c));

    c.selftest_required_mask = 0;        /* app sets (baro|imu|...) bits */
    c.selftest_timeout_ms    = 5000;

    c.arm_mode      = HK_ARM_AUTO_ALT;
    c.arm_altitude_m = 30.0f;
    c.arm_hold_ms    = 2000;

    c.release_freefall_g = 0.35f;
    c.release_hold_ms    = 300;
    c.release_vspeed_ms  = -8.0f;

    c.release_actuation_ms = 1500;

    c.landed_vspeed_ms   = 0.7f;
    c.landed_accel_tol_g = 0.15f;
    c.landed_hold_ms     = 4000;
    c.descent_timeout_ms = 180000;       /* 3 min hard cap on descent */

    c.recovery_after_ms = 10000;

    c.servo_hold_deg    = 0.0f;
    c.servo_release_deg = 180.0f;
    return c;
}

void hk_mission_init(hk_mission_t *m, const hk_mission_cfg_t *cfg)
{
    memset(m, 0, sizeof(*m));
    m->cfg   = (cfg != NULL) ? *cfg : hk_mission_default_cfg();
    m->state = HK_MISSION_BOOT;
}

/* Debounce helper: `timer` records when the condition became true.
 * Returns true once the condition has held for `hold_ms`. */
static bool sustained(uint32_t *timer, bool cond, uint32_t now, uint32_t hold_ms)
{
    if (!cond) {
        *timer = 0;
        return false;
    }
    if (*timer == 0) {
        *timer = (now == 0) ? 1u : now;   /* 0 means "inactive" */
        return hold_ms == 0;
    }
    return (now - *timer) >= hold_ms;
}

static void enter(hk_mission_t *m, hk_mission_state_t next, uint32_t now,
                  uint16_t arg, hk_mission_out_t *out)
{
    out->event      = true;
    out->event_from = m->state;
    out->event_arg  = arg;
    m->state      = next;
    m->t_enter_ms = now;
    m->cond_a_ms  = 0;
    m->cond_b_ms  = 0;
}

void hk_mission_step(hk_mission_t *m, const hk_mission_in_t *in,
                     hk_mission_out_t *out)
{
    if (m == NULL || in == NULL || out == NULL) {
        return;
    }
    const hk_mission_cfg_t *c = &m->cfg;
    const uint32_t now = in->t_ms;

    memset(out, 0, sizeof(*out));

    switch (m->state) {

    case HK_MISSION_BOOT:
        /* one tick to publish the initial state, then straight to selftest */
        enter(m, HK_MISSION_SELFTEST, now, HK_MISSION_ARG_NORMAL, out);
        break;

    case HK_MISSION_SELFTEST: {
        bool sensors_ok = (in->sensor_ok_mask & c->selftest_required_mask)
                          == c->selftest_required_mask;
        if (sensors_ok) {
            m->degraded = false;
            enter(m, HK_MISSION_ATTACHED, now, HK_MISSION_ARG_NORMAL, out);
        } else if ((now - m->t_enter_ms) >= c->selftest_timeout_ms) {
            /* fly with what we have; the log records that we're degraded */
            m->degraded = true;
            enter(m, HK_MISSION_ATTACHED, now,
                  HK_MISSION_ARG_SELFTEST_TIMEOUT, out);
        }
        break;
    }

    case HK_MISSION_ATTACHED: {
        if (in->arm_cmd) {
            enter(m, HK_MISSION_ARMED, now, HK_MISSION_ARG_CMD, out);
            break;
        }
        bool auto_alt = (c->arm_mode == HK_ARM_AUTO_ALT) && in->baro_ok &&
                        (in->alt_m >= c->arm_altitude_m);
        if (sustained(&m->cond_a_ms, auto_alt, now, c->arm_hold_ms)) {
            enter(m, HK_MISSION_ARMED, now, HK_MISSION_ARG_AUTO_ALT, out);
        }
        break;
    }

    case HK_MISSION_ARMED: {
        if (in->disarm_cmd) {
            enter(m, HK_MISSION_ATTACHED, now, HK_MISSION_ARG_CMD, out);
            break;
        }
        if (in->release_cmd) {
            enter(m, HK_MISSION_RELEASE, now, HK_MISSION_ARG_CMD, out);
            break;
        }
        /* separation detect: freefall (IMU) or strong sink rate (baro) */
        bool freefall = in->imu_ok && (in->accel_g < c->release_freefall_g);
        bool sinking  = in->baro_ok && (in->vspeed_ms <= c->release_vspeed_ms);
        if (sustained(&m->cond_a_ms, freefall, now, c->release_hold_ms)) {
            enter(m, HK_MISSION_RELEASE, now, HK_MISSION_ARG_FREEFALL, out);
        } else if (sustained(&m->cond_b_ms, sinking, now, c->release_hold_ms)) {
            enter(m, HK_MISSION_RELEASE, now, HK_MISSION_ARG_VSPEED, out);
        }
        break;
    }

    case HK_MISSION_RELEASE:
        /* actuators are commanded below; give them time to move */
        if ((now - m->t_enter_ms) >= c->release_actuation_ms) {
            enter(m, HK_MISSION_DESCENT, now, HK_MISSION_ARG_NORMAL, out);
        }
        break;

    case HK_MISSION_DESCENT: {
        /* landing = quiet on every sensor that is still alive */
        bool baro_quiet = fabsf(in->vspeed_ms) < c->landed_vspeed_ms;
        bool imu_quiet  = fabsf(in->accel_g - 1.0f) < c->landed_accel_tol_g;
        bool quiet;
        if (in->baro_ok && in->imu_ok)      { quiet = baro_quiet && imu_quiet; }
        else if (in->baro_ok)               { quiet = baro_quiet; }
        else if (in->imu_ok)                { quiet = imu_quiet; }
        else                                { quiet = false; }

        if (sustained(&m->cond_a_ms, quiet, now, c->landed_hold_ms)) {
            enter(m, HK_MISSION_LANDED, now, HK_MISSION_ARG_NORMAL, out);
        } else if ((now - m->t_enter_ms) >= c->descent_timeout_ms) {
            enter(m, HK_MISSION_LANDED, now,
                  HK_MISSION_ARG_TIMEOUT_FAILSAFE, out);
        }
        break;
    }

    case HK_MISSION_LANDED:
        if ((now - m->t_enter_ms) >= c->recovery_after_ms) {
            enter(m, HK_MISSION_RECOVERY, now, HK_MISSION_ARG_NORMAL, out);
        }
        break;

    case HK_MISSION_RECOVERY:
    default:
        /* terminal: beacon until the team picks the capsule up */
        break;
    }

    /* ---- level outputs for the (possibly new) state ---- */
    out->state = m->state;

    bool released = (m->state >= HK_MISSION_RELEASE);
    out->lock_engaged = !released;
    out->servo_deg    = released ? c->servo_release_deg : c->servo_hold_deg;

    switch (m->state) {
    case HK_MISSION_BOOT:
    case HK_MISSION_SELFTEST: out->buzzer_pattern = HK_BUZZ_BOOT;     break;
    case HK_MISSION_ATTACHED: out->buzzer_pattern = HK_BUZZ_OFF;      break;
    case HK_MISSION_ARMED:    out->buzzer_pattern = HK_BUZZ_ARMED;    break;
    case HK_MISSION_RELEASE:
    case HK_MISSION_DESCENT:  out->buzzer_pattern = HK_BUZZ_DESCENT;  break;
    case HK_MISSION_LANDED:   out->buzzer_pattern = HK_BUZZ_LANDED;   break;
    case HK_MISSION_RECOVERY: out->buzzer_pattern = HK_BUZZ_RECOVERY; break;
    default:                  out->buzzer_pattern = HK_BUZZ_OFF;      break;
    }
}
