#ifndef HK_FILTERS_H
#define HK_FILTERS_H

#include "common/units.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Complementary filter for roll/pitch from accelerometer + gyro.
 * Pure/host-testable. alpha ~0.98 trusts the gyro short-term and the
 * accelerometer long-term. Yaw is not observable from accel+gyro alone.
 */
typedef struct {
    float roll_deg;
    float pitch_deg;
    float alpha;
} hk_comp_filter_t;

void hk_comp_filter_init(hk_comp_filter_t *f, float alpha);

/* accel in m/s^2, gyro in rad/s, dt in seconds. */
void hk_comp_filter_update(hk_comp_filter_t *f, const hk_vec3f *accel,
                           const hk_vec3f *gyro, float dt);

/*
 * Low-pass filtered derivative: vertical speed from baro altitude samples.
 * y' = (x - x_prev) / dt, smoothed with a single-pole IIR (alpha in (0,1];
 * smaller alpha = smoother/slower). First sample initializes the state
 * without producing a spike. Pure/host-testable.
 */
typedef struct {
    float value;      /* filtered derivative (units/s) */
    float prev_x;
    float alpha;
    int   primed;
} hk_deriv_lpf_t;

void  hk_deriv_lpf_init(hk_deriv_lpf_t *f, float alpha);
/* Feed one sample; returns the filtered derivative. dt must be > 0. */
float hk_deriv_lpf_update(hk_deriv_lpf_t *f, float x, float dt);

#ifdef __cplusplus
}
#endif

#endif /* HK_FILTERS_H */
