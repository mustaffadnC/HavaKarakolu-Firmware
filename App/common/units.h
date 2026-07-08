#ifndef HK_UNITS_H
#define HK_UNITS_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Physical constants ---- */
#define HK_STD_PRESSURE_PA      101325.0f   /* ISA sea-level pressure */
#define HK_GRAVITY_MS2          9.80665f
#define HK_KELVIN_OFFSET        273.15f
#define HK_DEG2RAD              0.017453292519943295f
#define HK_RAD2DEG              57.29577951308232f

/* ---- Small vector type (SI units) ---- */
typedef struct { float x, y, z; } hk_vec3f;

/* ---- Generic helpers ---- */
#define HK_MIN(a, b)            (((a) < (b)) ? (a) : (b))
#define HK_MAX(a, b)            (((a) > (b)) ? (a) : (b))
#define HK_CLAMP(v, lo, hi)     HK_MAX((lo), HK_MIN((v), (hi)))
#define HK_ARRAY_LEN(a)         (sizeof(a) / sizeof((a)[0]))

/* Linear map of x in [in_lo,in_hi] to [out_lo,out_hi] (no clamping). */
static inline float hk_mapf(float x, float in_lo, float in_hi,
                            float out_lo, float out_hi)
{
    return out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

/*
 * Barometric (ISA troposphere) altitude in metres from absolute pressure [Pa]
 * relative to a reference sea-level pressure [Pa]. Use the measured ground
 * pressure as ref_pa to get height-above-launch.
 */
static inline float hk_altitude_from_pressure(float pressure_pa, float ref_pa)
{
    if (pressure_pa <= 0.0f || ref_pa <= 0.0f) {
        return 0.0f;
    }
    return 44330.0f * (1.0f - powf(pressure_pa / ref_pa, 0.1902949f));
}

#ifdef __cplusplus
}
#endif

#endif /* HK_UNITS_H */
