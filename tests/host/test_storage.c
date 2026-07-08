#include "services/storage/storage.h"
#include "hk_test.h"
#include "mocks/diskio_ram.h"

#include <stdlib.h>
#include <string.h>

#include "ff.h"

#define DISK_SECTORS 8192u   /* 4 MB RAM disk */

static uint8_t s_queue[4096];

/* ---- helpers ---- */

static void fresh_disk(void)
{
    HK_CHECK(hk_ramdisk_create(DISK_SECTORS));
    static uint8_t work[4096];
    HK_CHECK_EQ_INT(f_mkfs("", NULL, work, sizeof(work)), FR_OK);
}

static hk_rec_env_t make_env(uint32_t t_ms)
{
    hk_rec_env_t r;
    memset(&r, 0, sizeof(r));
    r.t_ms        = t_ms;
    r.mission_state = 2;             /* ATTACHED */
    r.press_pa    = 101325.0f;
    r.alt_m       = 1.5f;
    r.temp_bmp_c  = 25.0f;
    r.temp_sht1_c = 24.5f;
    r.rh_sht1     = 40.0f;
    r.temp_sht2_c = 26.0f;
    r.rh_sht2     = 42.0f;
    r.vbat_v      = 11.8f;
    r.soc         = 0.9f;
    return r;
}

/* Read a whole file into a malloc'd buffer. */
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

/* Parse frames with resync-on-garbage; returns count, fills type histogram. */
static int scan_frames(const uint8_t *buf, size_t len, int hist[8])
{
    int    count = 0;
    size_t pos   = 0;
    memset(hist, 0, 8 * sizeof(int));
    while (pos < len) {
        uint8_t type  = 0;
        size_t  flen  = hk_log_frame_parse(&buf[pos], len - pos, &type);
        if (flen == 0) {
            pos++;   /* resync: skip a byte, hunt for the next magic */
            continue;
        }
        if (type < 8) {
            hist[type]++;
        }
        count++;
        pos += flen;
    }
    return count;
}

/* ---- tests ---- */

static void test_happy_path_and_content(void)
{
    fresh_disk();

    hk_storage_t     st;
    hk_storage_cfg_t cfg = { .sync_period_ms = 1000, .retry_period_ms = 5000,
                             .imu_decim = 4, .fw_version = 0x0100,
                             .reset_reason = 3 };
    HK_CHECK_EQ_INT(hk_storage_init(&st, &cfg, s_queue, sizeof(s_queue)), HK_OK);

    hk_storage_service(&st, 0);
    HK_CHECK(hk_storage_mounted(&st));
    HK_CHECK_EQ_INT(st.session, 1);

    for (uint32_t i = 0; i < 5; ++i) {
        hk_rec_env_t e = make_env(i * 500u);
        hk_storage_push_env(&st, &e);
    }
    for (uint32_t i = 0; i < 20; ++i) {           /* decim 4 -> 5 records */
        hk_rec_imu_t r;
        memset(&r, 0, sizeof(r));
        r.t_ms = i * 10u;
        r.az   = 9.81f;
        hk_storage_push_imu(&st, &r);
    }
    for (uint32_t i = 0; i < 2; ++i) {
        hk_rec_gps_t g;
        memset(&g, 0, sizeof(g));
        g.t_ms    = i * 1000u;
        g.lat_deg = 39.925;
        g.lon_deg = 32.837;
        g.valid   = 1;
        hk_storage_push_gps(&st, &g);
    }
    hk_rec_event_t ev = { .t_ms = 2100, .from_state = 2, .to_state = 3, .arg = 0 };
    hk_storage_push_event(&st, &ev);

    hk_storage_service(&st, 100);    /* drain (event forces sync) */
    hk_storage_service(&st, 1200);   /* periodic sync */
    HK_CHECK_EQ_INT((long)st.dropped, 0L);
    hk_storage_close(&st);

    /* verify on a fresh mount */
    FATFS fs;
    HK_CHECK_EQ_INT(f_mount(&fs, "", 1), FR_OK);

    size_t   blen = 0;
    uint8_t *bin  = read_file("/LOGS/FL_0001.BIN", &blen);
    HK_CHECK(bin != NULL);
    int hist[8];
    int frames = scan_frames(bin, blen, hist);
    HK_CHECK_EQ_INT(frames, 1 + 5 + 5 + 2 + 1);
    HK_CHECK_EQ_INT(hist[HK_REC_META], 1);
    HK_CHECK_EQ_INT(hist[HK_REC_ENV], 5);
    HK_CHECK_EQ_INT(hist[HK_REC_IMU], 5);
    HK_CHECK_EQ_INT(hist[HK_REC_GPS], 2);
    HK_CHECK_EQ_INT(hist[HK_REC_EVENT], 1);
    free(bin);

    size_t clen = 0;
    char  *csv  = (char *)read_file("/LOGS/FL_0001.CSV", &clen);
    HK_CHECK(csv != NULL);
    int lines = 0;
    for (size_t i = 0; i < clen; ++i) {
        if (csv[i] == '\n') { lines++; }
    }
    HK_CHECK_EQ_INT(lines, 1 + 5 + 1);   /* header + env rows + event row */
    HK_CHECK(strncmp(csv, "t_ms,state,", 11) == 0);
    HK_CHECK(strstr(csv, "EVENT,ATTACHED->ARMED") != NULL);
    free(csv);

    (void)f_unmount("");
}

static void test_session_increment(void)
{
    /* same disk as previous test: FL_0001 exists -> new boot gets FL_0002 */
    hk_storage_t st;
    HK_CHECK_EQ_INT(hk_storage_init(&st, NULL, s_queue, sizeof(s_queue)), HK_OK);
    hk_storage_service(&st, 0);
    HK_CHECK(hk_storage_mounted(&st));
    HK_CHECK_EQ_INT(st.session, 2);
    hk_storage_close(&st);

    FATFS   fs;
    FILINFO fi;
    HK_CHECK_EQ_INT(f_mount(&fs, "", 1), FR_OK);
    HK_CHECK_EQ_INT(f_stat("/LOGS/FL_0002.BIN", &fi), FR_OK);
    (void)f_unmount("");
}

static void test_power_loss(void)
{
    fresh_disk();

    hk_storage_t st;
    hk_storage_cfg_t cfg = { .sync_period_ms = 1000 };
    HK_CHECK_EQ_INT(hk_storage_init(&st, &cfg, s_queue, sizeof(s_queue)), HK_OK);

    hk_storage_service(&st, 0);
    HK_CHECK(hk_storage_mounted(&st));

    hk_rec_env_t e1 = make_env(100);
    hk_storage_push_env(&st, &e1);
    hk_storage_service(&st, 200);    /* drained, not yet synced */
    hk_storage_service(&st, 1100);   /* periodic sync happens here */

    /* snapshot the card right after the sync */
    uint8_t *snap = (uint8_t *)malloc(hk_ramdisk_size());
    HK_CHECK(snap != NULL);
    hk_ramdisk_snapshot(snap);

    /* more data arrives but power dies before the next sync... */
    hk_rec_env_t e2 = make_env(1500);
    hk_storage_push_env(&st, &e2);
    hk_storage_service(&st, 1600);   /* drained; next sync would be at 2100 */
    hk_ramdisk_restore(snap);        /* --- POWER LOSS --- */
    free(snap);

    /* "reboot": verify everything up to the last sync is intact */
    FATFS fs;
    HK_CHECK_EQ_INT(f_mount(&fs, "", 1), FR_OK);
    size_t   blen = 0;
    uint8_t *bin  = read_file("/LOGS/FL_0001.BIN", &blen);
    HK_CHECK(bin != NULL);
    int hist[8];
    int frames = scan_frames(bin, blen, hist);
    HK_CHECK_EQ_INT(frames, 2);                    /* META + first ENV */
    HK_CHECK_EQ_INT(hist[HK_REC_ENV], 1);
    if (hist[HK_REC_ENV] == 1) {
        /* the surviving env record is e1, not e2 */
        size_t pos = 0;
        uint8_t type = 0;
        size_t  flen;
        while ((flen = hk_log_frame_parse(&bin[pos], blen - pos, &type)) != 0) {
            if (type == HK_REC_ENV) {
                hk_rec_env_t r;
                hk_log_decode_env(&bin[pos + 5], &r);
                HK_CHECK_EQ_INT((long)r.t_ms, 100L);
            }
            pos += flen;
        }
    }
    free(bin);
    (void)f_unmount("");
}

static void test_reader_resyncs_past_garbage(void)
{
    /* corrupt tail: reader must survive and still find valid frames */
    uint8_t buf[256];
    hk_rec_event_t ev = { .t_ms = 42, .from_state = 1, .to_state = 2, .arg = 7 };
    size_t n = 0;
    buf[n++] = 0xDE;  buf[n++] = 0xAD;              /* leading garbage */
    n += hk_log_frame_event(&buf[n], &ev);
    buf[n++] = 0x48;  buf[n++] = 0x00;              /* fake half-magic */
    n += hk_log_frame_event(&buf[n], &ev);
    buf[n++] = 0x48;  buf[n++] = 0x4B;  buf[n++] = 0x01; /* torn header */

    int hist[8];
    int frames = scan_frames(buf, n, hist);
    HK_CHECK_EQ_INT(frames, 2);
    HK_CHECK_EQ_INT(hist[HK_REC_EVENT], 2);
}

static void test_degraded_mode_and_recovery(void)
{
    fresh_disk();

    hk_storage_t st;
    hk_storage_cfg_t cfg = { .sync_period_ms = 500, .retry_period_ms = 5000 };
    HK_CHECK_EQ_INT(hk_storage_init(&st, &cfg, s_queue, sizeof(s_queue)), HK_OK);

    hk_storage_service(&st, 0);
    HK_CHECK(hk_storage_mounted(&st));

    /* card starts failing: the next sync detects it and degrades */
    hk_ramdisk_fail_after(0);
    hk_rec_env_t e = make_env(100);
    hk_storage_push_env(&st, &e);
    hk_storage_service(&st, 600);    /* drain + sync attempt -> failure */
    HK_CHECK(!hk_storage_mounted(&st));

    /* still failing: retry throttled, no crash */
    hk_storage_service(&st, 700);
    HK_CHECK(!hk_storage_mounted(&st));

    /* card behaves again: remount at the retry period as a NEW session */
    hk_ramdisk_fail_after(-1);
    hk_storage_service(&st, 6000);
    HK_CHECK(hk_storage_mounted(&st));
    HK_CHECK(st.session >= 2);
    hk_storage_close(&st);
}

static void test_queue_overflow_drops(void)
{
    fresh_disk();

    static uint8_t tiny[128];
    hk_storage_t st;
    HK_CHECK_EQ_INT(hk_storage_init(&st, NULL, tiny, sizeof(tiny)), HK_OK);

    for (uint32_t i = 0; i < 10; ++i) {
        hk_rec_env_t e = make_env(i);
        hk_storage_push_env(&st, &e);   /* 48-byte frames into 127 usable */
    }
    HK_CHECK(st.dropped > 0);

    hk_storage_service(&st, 0);   /* drains whatever fit, no crash */
    HK_CHECK(hk_storage_mounted(&st));
    hk_storage_close(&st);
}

static void test_frame_validation(void)
{
    uint8_t f[HK_LOG_MAX_FRAME];
    hk_rec_event_t ev = { .t_ms = 1, .from_state = 0, .to_state = 1, .arg = 0 };
    size_t len = hk_log_frame_event(f, &ev);
    HK_CHECK(len == 15);

    uint8_t type = 0;
    HK_CHECK(hk_log_frame_parse(f, len, &type) == len);
    HK_CHECK_EQ_INT(type, HK_REC_EVENT);

    HK_CHECK(hk_log_frame_parse(f, len - 1, &type) == 0);   /* truncated */
    f[7] ^= 0xFF;                                           /* corrupt payload */
    HK_CHECK(hk_log_frame_parse(f, len, &type) == 0);       /* bad CRC */
    f[7] ^= 0xFF;
    f[0] = 0x00;                                            /* bad magic */
    HK_CHECK(hk_log_frame_parse(f, len, &type) == 0);
}

int main(void)
{
    printf("test_storage\n");
    test_happy_path_and_content();
    test_session_increment();
    test_power_loss();
    test_reader_resyncs_past_garbage();
    test_degraded_mode_and_recovery();
    test_queue_overflow_drops();
    test_frame_validation();
    hk_ramdisk_destroy();
    return hk_test_summary();
}
