#ifndef HK_NMEA_H
#define HK_NMEA_H

#include <stdbool.h>
#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded GPS state, accumulated across RMC/GGA sentences. */
typedef struct {
    bool     valid;          /* position fix usable (RMC 'A' or GGA quality>0) */
    double   lat_deg;        /* +N / -S */
    double   lon_deg;        /* +E / -W */
    float    alt_m;          /* MSL altitude (GGA) */
    float    speed_mps;      /* ground speed (RMC) */
    float    course_deg;     /* course over ground (RMC) */
    uint8_t  satellites;     /* GGA */
    uint8_t  fix_quality;    /* GGA: 0 none, 1 GPS, 2 DGPS ... */

    /* UTC time/date (best-effort) */
    uint8_t  hour, minute;
    float    second;
    uint8_t  day, month;
    uint16_t year;
} hk_gps_fix_t;

typedef enum {
    HK_NMEA_NONE = 0,
    HK_NMEA_GGA,
    HK_NMEA_RMC,
    HK_NMEA_OTHER
} hk_nmea_type_t;

/* Streaming line assembler. */
#define HK_NMEA_LINE_MAX 100
typedef struct {
    char   line[HK_NMEA_LINE_MAX];
    size_t len;
    bool   in_sentence;
} hk_nmea_t;

void hk_nmea_init(hk_nmea_t *n);

/*
 * Feed one raw byte. When a full, checksum-valid sentence completes it is
 * parsed into *out and the sentence type is returned; otherwise HK_NMEA_NONE.
 */
hk_nmea_type_t hk_nmea_feed(hk_nmea_t *n, char c, hk_gps_fix_t *out);

/*
 * Parse a single complete sentence (leading '$', no trailing CR/LF) and update
 * *out. Verifies the XOR checksum. Pure/host-testable. Returns the type or
 * HK_NMEA_NONE on checksum/format failure.
 */
hk_nmea_type_t hk_nmea_parse_sentence(const char *sentence, hk_gps_fix_t *out);

/* Verify an NMEA sentence checksum ("$....*HH"). */
bool hk_nmea_checksum_ok(const char *sentence);

/* Convert NMEA ddmm.mmmm + hemisphere to decimal degrees. */
double hk_nmea_to_decimal(double ddmm, char hemi);

#ifdef __cplusplus
}
#endif

#endif /* HK_NMEA_H */
