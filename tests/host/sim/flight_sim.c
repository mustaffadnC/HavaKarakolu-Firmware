#include "sim/flight_sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

hk_sim_cfg_t hk_sim_default_cfg(void)
{
    hk_sim_cfg_t c;
    memset(&c, 0, sizeof(c));
    c.t_ground_ms     = 2000;
    c.climb_rate_ms   = 5.0f;
    c.release_alt_m   = 60.0f;
    c.t_freefall_ms   = 1000;
    c.chute_rate_ms   = 6.0f;
    c.ref_pressure_pa = 101325.0f;
    c.press_noise_pa  = 1.3f;
    c.accel_noise_g   = 0.01f;
    c.seed            = 0xC0FFEEu;
    c.lat0_deg        = 39.925;    /* Ankara-ish */
    c.lon0_deg        = 32.837;
    return c;
}

void hk_sim_init(hk_sim_t *s, const hk_sim_cfg_t *cfg)
{
    memset(s, 0, sizeof(*s));
    s->cfg   = (cfg != NULL) ? *cfg : hk_sim_default_cfg();
    s->phase = HK_SIM_GROUND;
    s->rng   = (s->cfg.seed == 0) ? 1u : s->cfg.seed;
}

/* LCG uniform in [0,1). */
static float frand(hk_sim_t *s)
{
    s->rng = s->rng * 1664525u + 1013904223u;
    return (float)(s->rng >> 8) / 16777216.0f;
}

/* Approximate standard normal (Irwin-Hall with 4 uniforms). */
static float gauss(hk_sim_t *s)
{
    return (frand(s) + frand(s) + frand(s) + frand(s) - 2.0f) * 1.7320508f;
}

/* Exact inverse of hk_altitude_from_pressure (ISA troposphere). */
static float pressure_from_alt(float alt_m, float ref_pa)
{
    return ref_pa * powf(1.0f - alt_m / 44330.0f, 1.0f / 0.1902949f);
}

static void change_phase(hk_sim_t *s, hk_sim_phase_t next)
{
    s->phase       = next;
    s->phase_t0_ms = s->t_ms;
}

void hk_sim_step(hk_sim_t *s, uint32_t dt_ms, hk_sim_sample_t *out)
{
    const float dt = (float)dt_ms / 1000.0f;
    s->t_ms += dt_ms;

    float accel_g = 1.0f;
    uint32_t in_phase = s->t_ms - s->phase_t0_ms;

    switch (s->phase) {
    case HK_SIM_GROUND:
        s->vspeed_ms = 0.0f;
        if (s->t_ms >= s->cfg.t_ground_ms) {
            change_phase(s, HK_SIM_CLIMB);
        }
        break;

    case HK_SIM_CLIMB:
        s->vspeed_ms = s->cfg.climb_rate_ms;
        s->alt_m    += s->vspeed_ms * dt;
        accel_g      = 1.02f;   /* mild carrier vibration bias */
        if (s->alt_m >= s->cfg.release_alt_m) {
            change_phase(s, HK_SIM_FREEFALL);
        }
        break;

    case HK_SIM_FREEFALL:
        s->vspeed_ms -= 9.80665f * dt;
        s->alt_m     += s->vspeed_ms * dt;
        accel_g       = 0.05f;
        if (in_phase >= s->cfg.t_freefall_ms) {
            change_phase(s, HK_SIM_CHUTE);
        }
        break;

    case HK_SIM_CHUTE:
        /* opening shock for the first 200 ms, then steady descent */
        if (in_phase < 200u) {
            accel_g = 2.5f;
        }
        s->vspeed_ms = -s->cfg.chute_rate_ms;
        s->alt_m    += s->vspeed_ms * dt;
        if (s->alt_m <= 0.0f) {
            s->alt_m = 0.0f;
            change_phase(s, HK_SIM_LANDED);
        }
        break;

    case HK_SIM_LANDED:
    default:
        s->vspeed_ms = 0.0f;
        s->alt_m     = 0.0f;
        accel_g      = (in_phase < 100u) ? 2.0f : 1.0f;   /* touchdown bump */
        break;
    }

    out->phase          = s->phase;
    out->alt_true_m     = s->alt_m;
    out->vspeed_true_ms = s->vspeed_ms;
    out->press_pa       = pressure_from_alt(s->alt_m, s->cfg.ref_pressure_pa) +
                          gauss(s) * s->cfg.press_noise_pa;
    out->accel_g        = accel_g + gauss(s) * s->cfg.accel_noise_g;
}

/* dd.dddddd -> "ddmm.mmmm" NMEA coordinate text. */
static double to_nmea_coord(double deg)
{
    double d = floor(deg);
    return d * 100.0 + (deg - d) * 60.0;
}

size_t hk_sim_nmea_gga(const hk_sim_t *s, char *buf, size_t cap)
{
    /* light position drift with descent (few meters, cosmetic) */
    double lat = s->cfg.lat0_deg + (double)s->alt_m * 1e-7;
    double lon = s->cfg.lon0_deg + (double)s->t_ms * 1e-9;

    uint32_t secs = s->t_ms / 1000u;
    char body[96];
    int  n = snprintf(body, sizeof(body),
                      "GPGGA,%02u%02u%02u.00,%09.4f,N,%010.4f,E,1,08,1.0,%.1f,M,0.0,M,,",
                      (unsigned)(secs / 3600u) % 24u,
                      (unsigned)(secs / 60u) % 60u,
                      (unsigned)(secs % 60u),
                      to_nmea_coord(lat), to_nmea_coord(lon),
                      (double)s->alt_m);
    if (n <= 0) {
        return 0;
    }

    uint8_t cs = 0;
    for (int i = 0; i < n; ++i) {
        cs ^= (uint8_t)body[i];
    }

    int total = snprintf(buf, cap, "$%s*%02X\r\n", body, cs);
    return (total > 0 && (size_t)total < cap) ? (size_t)total : 0;
}
