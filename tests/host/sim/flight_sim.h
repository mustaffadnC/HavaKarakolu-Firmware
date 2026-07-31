#ifndef HK_FLIGHT_SIM_H
#define HK_FLIGHT_SIM_H

/*
 * Deterministic flight-profile simulator for host tests.
 *
 * Truth model: ground idle -> carrier climb -> separation/freefall ->
 * parachute descent (with an opening spike) -> landing (with a bump) ->
 * still on the ground. Emits:
 *   - barometric pressure from the exact inverse of the ISA formula in
 *     common/units.h, plus Gaussian-ish noise (seeded LCG: reproducible)
 *   - |accel| in g with phase-appropriate signature and noise
 *   - synthetic $GPGGA sentences (valid checksums) for the real NMEA parser
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HK_SIM_GROUND = 0,
    HK_SIM_CLIMB,
    HK_SIM_FREEFALL,
    HK_SIM_CHUTE,
    HK_SIM_LANDED
} hk_sim_phase_t;

typedef struct {
    uint32_t t_ground_ms;      /* idle before the carrier starts climbing */
    float    climb_rate_ms;    /* carrier climb rate [m/s]                */
    float    release_alt_m;    /* separation altitude AGL [m]             */
    uint32_t t_freefall_ms;    /* freefall before the chute opens         */
    float    chute_rate_ms;    /* steady descent rate [m/s], positive     */
    float    ref_pressure_pa;  /* pressure at launch altitude             */
    float    press_noise_pa;   /* RMS pressure noise; default stays at the
                                * pessimistic 1.3 Pa the BMP280 showed, so
                                * the BMP581 (0.30 Pa at osr_p x8) only ever
                                * makes the mission logic's job easier    */
    float    accel_noise_g;    /* RMS accel noise                         */
    uint32_t seed;
    double   lat0_deg, lon0_deg;
} hk_sim_cfg_t;

typedef struct {
    hk_sim_cfg_t   cfg;
    hk_sim_phase_t phase;
    uint32_t       t_ms;
    uint32_t       phase_t0_ms;
    float          alt_m;      /* truth altitude AGL */
    float          vspeed_ms;  /* truth vertical speed */
    uint32_t       rng;
} hk_sim_t;

typedef struct {
    hk_sim_phase_t phase;
    float          alt_true_m;
    float          vspeed_true_ms;
    float          press_pa;   /* noisy measurement */
    float          accel_g;    /* noisy |a| measurement */
} hk_sim_sample_t;

hk_sim_cfg_t hk_sim_default_cfg(void);
void hk_sim_init(hk_sim_t *s, const hk_sim_cfg_t *cfg);

/* Advance the truth by dt_ms and produce one noisy sample. */
void hk_sim_step(hk_sim_t *s, uint32_t dt_ms, hk_sim_sample_t *out);

/* Write a checksummed "$GPGGA,...\r\n" for the current position into buf.
 * Returns the string length, or 0 if cap is too small. */
size_t hk_sim_nmea_gga(const hk_sim_t *s, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* HK_FLIGHT_SIM_H */
