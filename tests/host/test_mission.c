#include "services/mission/mission.h"
#include "hk_test.h"

#include <math.h>
#include <string.h>

#include "common/filters.h"

#define TICK_MS 50u

/* Sensor flag bits mirroring system_state (mission core only sees a mask). */
#define F_BARO (1u << 0)
#define F_IMU  (1u << 3)

typedef struct {
    hk_mission_t     m;
    hk_mission_in_t  in;
    hk_mission_out_t out;
    uint32_t         t;
    int              events;
    uint32_t         visited_mask;
} sim_t;

static void sim_init(sim_t *s, const hk_mission_cfg_t *cfg)
{
    memset(s, 0, sizeof(*s));
    hk_mission_init(&s->m, cfg);
    s->in.baro_ok        = true;
    s->in.imu_ok         = true;
    s->in.sensor_ok_mask = F_BARO | F_IMU;
    s->in.accel_g        = 1.0f;
    s->t                 = 100;   /* arbitrary boot time */
}

static void tick(sim_t *s)
{
    s->in.t_ms = s->t;
    hk_mission_step(&s->m, &s->in, &s->out);
    if (s->out.event) {
        s->events++;
        s->visited_mask |= 1u << s->out.event_from;
    }
    s->visited_mask |= 1u << s->out.state;
    /* commands are edge-triggered: consumed by one step */
    s->in.arm_cmd = s->in.disarm_cmd = s->in.release_cmd = false;
    s->t += TICK_MS;
}

/* Run until the machine reaches `target` or `max_ms` elapses. */
static bool run_until(sim_t *s, hk_mission_state_t target, uint32_t max_ms)
{
    uint32_t deadline = s->t + max_ms;
    while (s->t < deadline) {
        tick(s);
        if (s->m.state == target) {
            return true;
        }
    }
    return false;
}

static hk_mission_cfg_t test_cfg(void)
{
    hk_mission_cfg_t c = hk_mission_default_cfg();
    c.selftest_required_mask = F_BARO | F_IMU;
    return c;
}

static void test_nominal_full_flight(void)
{
    hk_mission_cfg_t c = test_cfg();
    sim_t s;
    sim_init(&s, &c);

    /* BOOT -> SELFTEST -> ATTACHED (sensors ok immediately) */
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_SELFTEST);
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ATTACHED);
    HK_CHECK(!s.m.degraded);
    HK_CHECK(s.out.lock_engaged);                 /* fail-safe: hold */
    HK_CHECK(fabsf(s.out.servo_deg - c.servo_hold_deg) < 0.1f);

    /* carrier climb: 5 m/s; auto-arm at 30 m sustained 2 s */
    uint32_t t_climb_start = s.t;
    while (s.m.state == HK_MISSION_ATTACHED && s.t < t_climb_start + 60000u) {
        s.in.alt_m    = 5.0f * (float)(s.t - t_climb_start) / 1000.0f;
        s.in.vspeed_ms = 5.0f;
        tick(&s);
    }
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_AUTO_ALT);
    /* 30 m at t+6 s, hold 2 s -> armed no earlier than 8 s into the climb */
    HK_CHECK(s.t - t_climb_start >= 8000u);
    HK_CHECK_EQ_INT(s.out.buzzer_pattern, HK_BUZZ_ARMED);

    /* separation: freefall 0.1 g sustained 300 ms */
    s.in.accel_g  = 0.1f;
    s.in.vspeed_ms = -2.0f;
    HK_CHECK(run_until(&s, HK_MISSION_RELEASE, 2000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_FREEFALL);
    HK_CHECK(!s.out.lock_engaged);                /* solenoid released */
    HK_CHECK(fabsf(s.out.servo_deg - c.servo_release_deg) < 0.1f);

    /* parachute opens: 1 g again, sinking at 6 m/s; actuation dwell 1.5 s */
    s.in.accel_g   = 1.0f;
    s.in.vspeed_ms = -6.0f;
    HK_CHECK(run_until(&s, HK_MISSION_DESCENT, 3000));

    /* touch down: quiet on both sensors, hold 4 s */
    s.in.vspeed_ms = 0.0f;
    s.in.alt_m     = 0.0f;
    HK_CHECK(run_until(&s, HK_MISSION_LANDED, 10000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_NORMAL);

    /* recovery beacon after 10 s */
    HK_CHECK(run_until(&s, HK_MISSION_RECOVERY, 15000));
    HK_CHECK_EQ_INT(s.out.buzzer_pattern, HK_BUZZ_RECOVERY);

    /* all 8 states visited, exactly 7 transitions */
    HK_CHECK_EQ_INT((long)s.visited_mask, (1L << HK_MISSION_COUNT) - 1L);
    HK_CHECK_EQ_INT(s.events, 7);
}

static void test_debounce_no_release_on_spike(void)
{
    hk_mission_cfg_t c = test_cfg();
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    s.in.arm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);

    /* single 50 ms freefall spike (< 300 ms hold): must NOT release */
    s.in.accel_g = 0.05f;
    tick(&s);
    s.in.accel_g = 1.0f;
    for (int i = 0; i < 40; ++i) {
        tick(&s);
    }
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);

    /* sustained freefall: releases */
    s.in.accel_g = 0.05f;
    HK_CHECK(run_until(&s, HK_MISSION_RELEASE, 1000));
}

static void test_selftest_timeout_degraded(void)
{
    hk_mission_cfg_t c = test_cfg();
    c.selftest_timeout_ms = 1000;
    sim_t s;
    sim_init(&s, &c);
    s.in.sensor_ok_mask = 0;          /* nothing passes */
    s.in.baro_ok = s.in.imu_ok = false;

    HK_CHECK(run_until(&s, HK_MISSION_ATTACHED, 3000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_SELFTEST_TIMEOUT);
    HK_CHECK(s.m.degraded);
}

static void test_baro_only_release(void)
{
    hk_mission_cfg_t c = test_cfg();
    c.selftest_required_mask = F_BARO;    /* IMU is known-dead on this run */
    sim_t s;
    sim_init(&s, &c);
    s.in.imu_ok = false;              /* IMU dead: vspeed path must work */
    s.in.sensor_ok_mask = F_BARO;
    s.in.accel_g = 0.0f;              /* garbage from a dead IMU: ignored */
    HK_CHECK(run_until(&s, HK_MISSION_ATTACHED, 1000));
    s.in.arm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);

    s.in.vspeed_ms = -12.0f;          /* strong sink after separation */
    HK_CHECK(run_until(&s, HK_MISSION_RELEASE, 2000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_VSPEED);
}

static void test_imu_only_landing(void)
{
    hk_mission_cfg_t c = test_cfg();
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    s.in.arm_cmd = true;
    tick(&s);
    s.in.release_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_RELEASE);
    HK_CHECK(run_until(&s, HK_MISSION_DESCENT, 3000));

    /* baro dies during descent: landing decided from accel quietness */
    s.in.baro_ok   = false;
    s.in.vspeed_ms = -99.0f;          /* garbage: must be ignored */
    s.in.accel_g   = 1.02f;
    HK_CHECK(run_until(&s, HK_MISSION_LANDED, 10000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_NORMAL);
}

static void test_all_sensors_dead_failsafe_timeout(void)
{
    hk_mission_cfg_t c = test_cfg();
    c.descent_timeout_ms = 3000;
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    s.in.arm_cmd = true;
    tick(&s);
    s.in.release_cmd = true;
    tick(&s);
    HK_CHECK(run_until(&s, HK_MISSION_DESCENT, 3000));

    s.in.baro_ok = s.in.imu_ok = false;   /* nothing to decide landing with */
    HK_CHECK(run_until(&s, HK_MISSION_LANDED, 5000));
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_TIMEOUT_FAILSAFE);
}

static void test_disarm_and_rearm(void)
{
    hk_mission_cfg_t c = test_cfg();
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    s.in.arm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);

    s.in.disarm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ATTACHED);
    HK_CHECK_EQ_INT(s.out.event_arg, HK_MISSION_ARG_CMD);
    HK_CHECK(s.out.lock_engaged);     /* still safely held */

    s.in.arm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);
}

static void test_landing_hysteresis_updraft(void)
{
    hk_mission_cfg_t c = test_cfg();
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    s.in.arm_cmd = true;
    tick(&s);
    s.in.release_cmd = true;
    tick(&s);
    HK_CHECK(run_until(&s, HK_MISSION_DESCENT, 3000));

    /* 2 s of quiet (< 4 s hold), then an updraft: timer must reset */
    s.in.vspeed_ms = 0.1f;
    s.in.accel_g   = 1.0f;
    for (int i = 0; i < 40; ++i) { tick(&s); }        /* 2 s quiet */
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_DESCENT);
    s.in.vspeed_ms = -3.0f;                           /* moving again */
    for (int i = 0; i < 20; ++i) { tick(&s); }
    s.in.vspeed_ms = 0.1f;
    for (int i = 0; i < 40; ++i) { tick(&s); }        /* another 2 s quiet */
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_DESCENT);   /* still not landed */
    for (int i = 0; i < 50; ++i) { tick(&s); }        /* complete the 4 s */
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_LANDED);
}

static void test_no_auto_arm_when_external_mode(void)
{
    hk_mission_cfg_t c = test_cfg();
    c.arm_mode = HK_ARM_EXTERNAL;
    sim_t s;
    sim_init(&s, &c);
    tick(&s); tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ATTACHED);

    s.in.alt_m = 1000.0f;             /* way above the auto-arm altitude */
    for (int i = 0; i < 100; ++i) { tick(&s); }
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ATTACHED);

    s.in.arm_cmd = true;
    tick(&s);
    HK_CHECK_EQ_INT(s.m.state, HK_MISSION_ARMED);
}

static void test_deriv_lpf(void)
{
    hk_deriv_lpf_t f;
    hk_deriv_lpf_init(&f, 1.0f);      /* no smoothing: raw derivative */

    HK_CHECK(fabsf(hk_deriv_lpf_update(&f, 100.0f, 0.1f)) < 1e-6f); /* prime */
    /* 100 -> 99.5 in 0.1 s = -5 m/s */
    HK_CHECK(fabsf(hk_deriv_lpf_update(&f, 99.5f, 0.1f) - (-5.0f)) < 1e-3f);

    /* smoothing: a single step is attenuated by alpha */
    hk_deriv_lpf_init(&f, 0.2f);
    (void)hk_deriv_lpf_update(&f, 0.0f, 0.1f);
    float v = hk_deriv_lpf_update(&f, 1.0f, 0.1f);   /* raw = 10 m/s */
    HK_CHECK(fabsf(v - 2.0f) < 1e-3f);               /* 0.2 * 10 */
}

int main(void)
{
    printf("test_mission\n");
    test_nominal_full_flight();
    test_debounce_no_release_on_spike();
    test_selftest_timeout_degraded();
    test_baro_only_release();
    test_imu_only_landing();
    test_all_sensors_dead_failsafe_timeout();
    test_disarm_and_rearm();
    test_landing_hysteresis_updraft();
    test_no_auto_arm_when_external_mode();
    test_deriv_lpf();
    return hk_test_summary();
}
