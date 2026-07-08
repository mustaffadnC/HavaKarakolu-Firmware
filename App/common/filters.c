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
