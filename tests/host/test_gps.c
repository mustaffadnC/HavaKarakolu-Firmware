#include "drivers/gps_ublox/nmea.h"
#include "hk_test.h"

#include <math.h>
#include <string.h>

int main(void)
{
    printf("test_gps\n");

    /* Canonical example sentences (valid checksums). */
    const char *gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    const char *rmc =
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";

    HK_CHECK(hk_nmea_checksum_ok(gga));
    HK_CHECK(hk_nmea_checksum_ok(rmc));

    /* Corrupted checksum must fail. */
    char bad[80];
    strcpy(bad, gga);
    bad[strlen(bad) - 1] = '0';   /* flip last hex digit */
    HK_CHECK(!hk_nmea_checksum_ok(bad));

    hk_gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));

    HK_CHECK_EQ_INT(hk_nmea_parse_sentence(gga, &fix), HK_NMEA_GGA);
    HK_CHECK(fix.valid);
    HK_CHECK_EQ_INT(fix.fix_quality, 1);
    HK_CHECK_EQ_INT(fix.satellites, 8);
    HK_CHECK(fabs(fix.lat_deg - 48.1173) < 0.001);
    HK_CHECK(fabs(fix.lon_deg - 11.51667) < 0.001);
    HK_CHECK(fabsf(fix.alt_m - 545.4f) < 0.1f);
    HK_CHECK_EQ_INT(fix.hour, 12);
    HK_CHECK_EQ_INT(fix.minute, 35);

    memset(&fix, 0, sizeof(fix));
    HK_CHECK_EQ_INT(hk_nmea_parse_sentence(rmc, &fix), HK_NMEA_RMC);
    HK_CHECK(fix.valid);
    HK_CHECK(fabsf(fix.speed_mps - 11.52f) < 0.1f);   /* 22.4 kn */
    HK_CHECK(fabsf(fix.course_deg - 84.4f) < 0.1f);
    HK_CHECK_EQ_INT(fix.day, 23);
    HK_CHECK_EQ_INT(fix.month, 3);
    HK_CHECK_EQ_INT(fix.year, 1994);

    /* Streaming feed: byte-by-byte assembly + parse. */
    hk_nmea_t n;
    hk_nmea_init(&n);
    memset(&fix, 0, sizeof(fix));
    hk_nmea_type_t got = HK_NMEA_NONE;
    for (const char *p = gga; *p; ++p) {
        hk_nmea_type_t t = hk_nmea_feed(&n, *p, &fix);
        if (t != HK_NMEA_NONE) { got = t; }
    }
    got = hk_nmea_feed(&n, '\r', &fix);
    HK_CHECK_EQ_INT(got, HK_NMEA_GGA);
    HK_CHECK(fix.valid);

    return hk_test_summary();
}
