/*
 * End-to-end flight test on the host: the ONLY test that exercises the whole
 * chain as a system before hardware exists.
 *
 *   flight_sim (truth + noise, fixed seed)
 *     -> ISA altitude + vertical-speed filter   (common/units, filters)
 *     -> mission state machine                  (services/mission)
 *     -> storage records on a FatFs RAM disk    (services/storage)
 *     -> synthetic NMEA through the real parser (drivers/gps_ublox/nmea)
 *     -> read the BIN back: every frame CRC-clean, EVENT sequence == the
 *        expected mission profile with sane timing
 */
#include "hk_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common/filters.h"
#include "common/units.h"
#include "drivers/gps_ublox/nmea.h"
#include "mocks/diskio_ram.h"
#include "services/mission/mission.h"
#include "services/storage/storage.h"
#include "sim/flight_sim.h"

#define F_BARO (1u << 0)
#define F_IMU  (1u << 3)

#define TICK_MS      50u
#define ENV_MS       500u
#define GPS_MS       1000u
#define SERVICE_MS   100u

static uint8_t s_queue[8192];

static uint8_t *read_file(const char *path, size_t *out_len)
{
    FIL f;
    if (f_open(&f, path, FA_READ) != FR_OK) {
        return NULL;
    }
    size_t   size = (size_t)f_size(&f);
    uint8_t *buf  = (uint8_t *)malloc(size ? size : 1);
    UINT     br   = 0;
    if (f_read(&f, buf, (UINT)size, &br) != FR_OK || br != size) {
        free(buf);
        (void)f_close(&f);
        return NULL;
    }
    (void)f_close(&f);
    *out_len = size;
    return buf;
}

typedef struct {
    hk_rec_event_t ev[16];
    int            n_events;
    int            hist[8];
    int            resyncs;
} scan_result_t;

static int scan_bin(const uint8_t *buf, size_t len, scan_result_t *r)
{
    int    count = 0;
    size_t pos   = 0;
    memset(r, 0, sizeof(*r));
    while (pos < len) {
        uint8_t type = 0;
        size_t  flen = hk_log_frame_parse(&buf[pos], len - pos, &type);
        if (flen == 0) {
            pos++;
            r->resyncs++;
            continue;
        }
        if (type < 8) {
            r->hist[type]++;
        }
        if (type == HK_REC_EVENT && r->n_events < 16) {
            hk_log_decode_event(&buf[pos + 5], &r->ev[r->n_events++]);
        }
        count++;
        pos += flen;
    }
    return count;
}

int main(void)
{
    printf("test_flight_e2e\n");

    /* ---- world ---- */
    hk_sim_t sim;
    hk_sim_init(&sim, NULL);          /* deterministic default profile */

    /* ---- storage on a fresh card ---- */
    HK_CHECK(hk_ramdisk_create(8192));
    static uint8_t work[4096];
    HK_CHECK_EQ_INT(f_mkfs("", NULL, work, sizeof(work)), FR_OK);

    hk_storage_t st;
    hk_storage_cfg_t scfg = { .sync_period_ms = 1000, .imu_decim = 4,
                              .fw_version = 0x0200 };
    HK_CHECK_EQ_INT(hk_storage_init(&st, &scfg, s_queue, sizeof(s_queue)), HK_OK);

    /* ---- mission ---- */
    hk_mission_cfg_t mcfg = hk_mission_default_cfg();
    mcfg.selftest_required_mask = F_BARO | F_IMU;
    hk_mission_t mission;
    hk_mission_init(&mission, &mcfg);

    /* ---- env pipeline state ---- */
    hk_deriv_lpf_t vs;
    hk_deriv_lpf_init(&vs, 0.3f);
    float alt_m = 0.0f, vspeed = 0.0f;

    /* ---- gps pipeline ---- */
    hk_nmea_t nmea;
    hk_nmea_init(&nmea);
    hk_gps_fix_t fix;
    memset(&fix, 0, sizeof(fix));

    int pushed_env = 0, pushed_gps = 0, pushed_imu = 0;
    uint32_t t = 0, t_armed = 0, t_release = 0, t_landed = 0, t_recovery = 0;
    hk_mission_out_t out;
    memset(&out, 0, sizeof(out));

    const uint32_t MAX_MS = 120000;
    while (t < MAX_MS) {
        t += TICK_MS;

        hk_sim_sample_t smp;
        hk_sim_step(&sim, TICK_MS, &smp);

        /* env task @2 Hz: pressure -> altitude -> vspeed -> record */
        if ((t % ENV_MS) == 0u) {
            alt_m  = hk_altitude_from_pressure(smp.press_pa, 101325.0f);
            vspeed = hk_deriv_lpf_update(&vs, alt_m, (float)ENV_MS / 1000.0f);
            hk_rec_env_t er = {
                .t_ms = t, .mission_state = (uint8_t)mission.state,
                .press_pa = smp.press_pa, .alt_m = alt_m,
                .temp_bmp_c = 25.0f, .temp_sht1_c = 24.0f, .rh_sht1 = 40.0f,
                .temp_sht2_c = 26.0f, .rh_sht2 = 45.0f,
                .vbat_v = 11.7f, .soc = 0.85f,
            };
            hk_storage_push_env(&st, &er);
            pushed_env++;
        }

        /* imu task @ tick rate (storage decimates to 1-of-4) */
        hk_rec_imu_t ir = { .t_ms = t, .az = smp.accel_g * 9.80665f };
        hk_storage_push_imu(&st, &ir);
        pushed_imu++;

        /* gps @1 Hz through the REAL byte-level NMEA parser */
        if ((t % GPS_MS) == 0u) {
            char sentence[128];
            size_t n = hk_sim_nmea_gga(&sim, sentence, sizeof(sentence));
            HK_CHECK(n > 0);
            hk_nmea_type_t got = HK_NMEA_NONE;
            for (size_t i = 0; i < n; ++i) {
                hk_nmea_type_t r = hk_nmea_feed(&nmea, sentence[i], &fix);
                if (r != HK_NMEA_NONE) { got = r; }
            }
            HK_CHECK_EQ_INT(got, HK_NMEA_GGA);
            hk_rec_gps_t gr = {
                .t_ms = t, .lat_deg = fix.lat_deg, .lon_deg = fix.lon_deg,
                .alt_m = fix.alt_m, .satellites = fix.satellites,
                .fix_quality = fix.fix_quality, .valid = 1,
            };
            hk_storage_push_gps(&st, &gr);
            pushed_gps++;
        }

        /* mission task @ tick rate */
        hk_mission_in_t in = {
            .t_ms = t, .alt_m = alt_m, .vspeed_ms = vspeed,
            .accel_g = smp.accel_g,
            .baro_ok = true, .imu_ok = true,
            .sensor_ok_mask = F_BARO | F_IMU,
        };
        hk_mission_step(&mission, &in, &out);
        if (out.event) {
            hk_rec_event_t ev = { .t_ms = t,
                                  .from_state = (uint8_t)out.event_from,
                                  .to_state = (uint8_t)out.state,
                                  .arg = out.event_arg };
            hk_storage_push_event(&st, &ev);
            if (out.state == HK_MISSION_ARMED)    { t_armed    = t; }
            if (out.state == HK_MISSION_RELEASE)  { t_release  = t; }
            if (out.state == HK_MISSION_LANDED)   { t_landed   = t; }
            if (out.state == HK_MISSION_RECOVERY) { t_recovery = t; }
        }

        /* actuator sanity: the coil is energized ONLY while releasing */
        if (mission.state == HK_MISSION_RELEASE) {
            HK_CHECK(!out.lock_engaged);
        } else {
            HK_CHECK(out.lock_engaged);
        }

        /* storage task @10 Hz */
        if ((t % SERVICE_MS) == 0u) {
            hk_storage_service(&st, t);
        }

        if (mission.state == HK_MISSION_RECOVERY && t > t_recovery + 2000u) {
            break;
        }
    }

    HK_CHECK_EQ_INT(mission.state, HK_MISSION_RECOVERY);
    HK_CHECK_EQ_INT((long)st.dropped, 0L);
    hk_storage_service(&st, t + 100u);
    hk_storage_close(&st);

    /* ---- timing sanity (profile math, generous tolerances) ----
     * climb starts at 2 s (5 m/s): 30 m at 8 s + 2 s hold  -> armed ~10 s
     * 60 m at 14 s -> freefall detect ~ +350 ms            -> release ~14.4 s
     * chute: ~55 m @ 6 m/s -> touchdown ~24 s; vspeed decay + 4 s hold */
    HK_CHECK(t_armed >= 9000u && t_armed <= 12000u);
    HK_CHECK(t_release >= 14000u && t_release <= 16000u);
    HK_CHECK(t_landed >= 26000u && t_landed <= 40000u);
    HK_CHECK_EQ_INT((long)(t_recovery - t_landed),
                    (long)mcfg.recovery_after_ms);

    /* ---- read the flight log back ---- */
    FATFS fs;
    HK_CHECK_EQ_INT(f_mount(&fs, "", 1), FR_OK);
    size_t   blen = 0;
    uint8_t *bin  = read_file("/LOGS/FL_0001.BIN", &blen);
    HK_CHECK(bin != NULL);

    scan_result_t r;
    int frames = scan_bin(bin, blen, &r);
    free(bin);

    HK_CHECK_EQ_INT(r.resyncs, 0);                     /* no torn bytes */
    HK_CHECK_EQ_INT(r.hist[HK_REC_META], 1);
    HK_CHECK_EQ_INT(r.hist[HK_REC_ENV], pushed_env);
    HK_CHECK_EQ_INT(r.hist[HK_REC_GPS], pushed_gps);
    HK_CHECK_EQ_INT(r.hist[HK_REC_IMU], pushed_imu / 4);
    HK_CHECK_EQ_INT(r.hist[HK_REC_EVENT], 7);
    HK_CHECK_EQ_INT(frames, 1 + pushed_env + pushed_gps + pushed_imu / 4 + 7);

    /* EVENT sequence == the full mission profile, in order */
    static const uint8_t want[7][2] = {
        { HK_MISSION_BOOT,     HK_MISSION_SELFTEST },
        { HK_MISSION_SELFTEST, HK_MISSION_ATTACHED },
        { HK_MISSION_ATTACHED, HK_MISSION_ARMED    },
        { HK_MISSION_ARMED,    HK_MISSION_RELEASE  },
        { HK_MISSION_RELEASE,  HK_MISSION_DESCENT  },
        { HK_MISSION_DESCENT,  HK_MISSION_LANDED   },
        { HK_MISSION_LANDED,   HK_MISSION_RECOVERY },
    };
    HK_CHECK_EQ_INT(r.n_events, 7);
    for (int i = 0; i < 7 && i < r.n_events; ++i) {
        HK_CHECK_EQ_INT(r.ev[i].from_state, want[i][0]);
        HK_CHECK_EQ_INT(r.ev[i].to_state, want[i][1]);
    }
    if (r.n_events == 7) {
        HK_CHECK_EQ_INT(r.ev[2].arg, HK_MISSION_ARG_AUTO_ALT);
        HK_CHECK_EQ_INT(r.ev[3].arg, HK_MISSION_ARG_FREEFALL);
        HK_CHECK_EQ_INT(r.ev[5].arg, HK_MISSION_ARG_NORMAL);
    }

    /* GPS really went through the parser */
    HK_CHECK(fabs(fix.lat_deg - 39.925) < 0.01);
    HK_CHECK(fabs(fix.lon_deg - 32.837) < 0.01);
    HK_CHECK_EQ_INT(fix.fix_quality, 1);

    (void)f_unmount("");
    hk_ramdisk_destroy();
    return hk_test_summary();
}
