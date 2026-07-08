#include "common/filters.h"

#include <math.h>

void hk_comp_filter_init(hk_comp_filter_t *f, float alpha)
{
    f->roll_deg  = 0.0f;
    f->pitch_deg = 0.0f;
    f->alpha     = HK_CLAMP(alpha, 0.0f, 1.0f);
}

void hk_comp_filter_update(hk_comp_filter_t *f, const hk_vec3f *accel,
                           const hk_vec3f *gyro, float dt)
{
    float roll_acc  = atan2f(accel->y, accel->z) * HK_RAD2DEG;
    float pitch_acc = atan2f(-accel->x,
                             sqrtf(accel->y * accel->y + accel->z * accel->z))
                      * HK_RAD2DEG;

    float gx_deg = gyro->x * HK_RAD2DEG;  /* deg/s */
    float gy_deg = gyro->y * HK_RAD2DEG;

    f->roll_deg  = f->alpha * (f->roll_deg  + gx_deg * dt) + (1.0f - f->alpha) * roll_acc;
    f->pitch_deg = f->alpha * (f->pitch_deg + gy_deg * dt) + (1.0f - f->alpha) * pitch_acc;
}

void hk_deriv_lpf_init(hk_deriv_lpf_t *f, float alpha)
{
    f->value  = 0.0f;
    f->prev_x = 0.0f;
    f->alpha  = HK_CLAMP(alpha, 0.001f, 1.0f);
    f->primed = 0;
}

float hk_deriv_lpf_update(hk_deriv_lpf_t *f, float x, float dt)
{
    if (dt <= 0.0f) {
        return f->value;
    }
    if (!f->primed) {
        f->prev_x = x;
        f->primed = 1;
        return 0.0f;
    }
    float raw = (x - f->prev_x) / dt;
    f->prev_x = x;
    f->value += f->alpha * (raw - f->value);
    return f->value;
}
