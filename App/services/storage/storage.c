#include "services/storage/storage.h"

#include <stdio.h>
#include <string.h>

#include "common/crc.h"
#include "services/mission/mission.h"

/* ------------------------------------------------ LE serialization ---- */

static uint8_t *put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
    return p + 2;
}

static uint8_t *put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
    return p + 4;
}

static uint8_t *put_f32(uint8_t *p, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return put_u32(p, bits);
}

static uint8_t *put_f64(uint8_t *p, double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    (void)put_u32(p, (uint32_t)(bits & 0xFFFFFFFFu));
    (void)put_u32(p + 4, (uint32_t)(bits >> 32));
    return p + 8;
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float get_f32(const uint8_t *p)
{
    uint32_t bits = get_u32(p);
    float v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

/* ------------------------------------------------------ framing ---- */

static size_t finish_frame(uint8_t *out, uint8_t type, size_t payload_len)
{
    out[0] = HK_LOG_MAGIC0;
    out[1] = HK_LOG_MAGIC1;
    out[2] = HK_LOG_VERSION;
    out[3] = type;
    out[4] = (uint8_t)payload_len;
    uint16_t crc = hk_crc16_ccitt(&out[2], payload_len + 3u);
    (void)put_u16(&out[5 + payload_len], crc);
    return 5u + payload_len + 2u;
}

size_t hk_log_frame_meta(uint8_t *out, const hk_rec_meta_t *r)
{
    if (out == NULL || r == NULL) {
        return 0;
    }
    uint8_t *p = &out[5];
    p = put_u32(p, r->t_ms);
    p = put_u16(p, r->fw_version);
    *p++ = r->reset_reason;
    *p++ = r->log_version;
    return finish_frame(out, HK_REC_META, 8u);
}

size_t hk_log_frame_env(uint8_t *out, const hk_rec_env_t *r)
{
    if (out == NULL || r == NULL) {
        return 0;
    }
    uint8_t *p = &out[5];
    p = put_u32(p, r->t_ms);
    *p++ = r->mission_state;
    p = put_f32(p, r->press_pa);
    p = put_f32(p, r->alt_m);
    p = put_f32(p, r->temp_bmp_c);
    p = put_f32(p, r->temp_sht1_c);
    p = put_f32(p, r->rh_sht1);
    p = put_f32(p, r->temp_sht2_c);
    p = put_f32(p, r->rh_sht2);
    p = put_f32(p, r->vbat_v);
    p = put_f32(p, r->soc);
    return finish_frame(out, HK_REC_ENV, 41u);
}

size_t hk_log_frame_imu(uint8_t *out, const hk_rec_imu_t *r)
{
    if (out == NULL || r == NULL) {
        return 0;
    }
    uint8_t *p = &out[5];
    p = put_u32(p, r->t_ms);
    p = put_f32(p, r->ax); p = put_f32(p, r->ay); p = put_f32(p, r->az);
    p = put_f32(p, r->gx); p = put_f32(p, r->gy); p = put_f32(p, r->gz);
    p = put_f32(p, r->roll_deg);
    p = put_f32(p, r->pitch_deg);
    return finish_frame(out, HK_REC_IMU, 36u);
}

size_t hk_log_frame_gps(uint8_t *out, const hk_rec_gps_t *r)
{
    if (out == NULL || r == NULL) {
        return 0;
    }
    uint8_t *p = &out[5];
    p = put_u32(p, r->t_ms);
    p = put_f64(p, r->lat_deg);
    p = put_f64(p, r->lon_deg);
    p = put_f32(p, r->alt_m);
    p = put_f32(p, r->speed_mps);
    p = put_f32(p, r->course_deg);
    *p++ = r->satellites;
    *p++ = r->fix_quality;
    *p++ = r->valid;
    return finish_frame(out, HK_REC_GPS, 35u);
}

size_t hk_log_frame_event(uint8_t *out, const hk_rec_event_t *r)
{
    if (out == NULL || r == NULL) {
        return 0;
    }
    uint8_t *p = &out[5];
    p = put_u32(p, r->t_ms);
    *p++ = r->from_state;
    *p++ = r->to_state;
    p = put_u16(p, r->arg);
    return finish_frame(out, HK_REC_EVENT, 8u);
}

size_t hk_log_frame_parse(const uint8_t *buf, size_t len, uint8_t *type)
{
    if (buf == NULL || len < 7u) {
        return 0;
    }
    if (buf[0] != HK_LOG_MAGIC0 || buf[1] != HK_LOG_MAGIC1 ||
        buf[2] != HK_LOG_VERSION) {
        return 0;
    }
    size_t payload_len = buf[4];
    size_t total = 5u + payload_len + 2u;
    if (len < total || total > HK_LOG_MAX_FRAME) {
        return 0;
    }
    uint16_t crc_calc = hk_crc16_ccitt(&buf[2], payload_len + 3u);
    if (crc_calc != get_u16(&buf[5 + payload_len])) {
        return 0;
    }
    if (type != NULL) {
        *type = buf[3];
    }
    return total;
}

void hk_log_decode_env(const uint8_t *payload, hk_rec_env_t *r)
{
    r->t_ms          = get_u32(&payload[0]);
    r->mission_state = payload[4];
    r->press_pa      = get_f32(&payload[5]);
    r->alt_m         = get_f32(&payload[9]);
    r->temp_bmp_c    = get_f32(&payload[13]);
    r->temp_sht1_c   = get_f32(&payload[17]);
    r->rh_sht1       = get_f32(&payload[21]);
    r->temp_sht2_c   = get_f32(&payload[25]);
    r->rh_sht2       = get_f32(&payload[29]);
    r->vbat_v        = get_f32(&payload[33]);
    r->soc           = get_f32(&payload[37]);
}

void hk_log_decode_event(const uint8_t *payload, hk_rec_event_t *r)
{
    r->t_ms       = get_u32(&payload[0]);
    r->from_state = payload[4];
    r->to_state   = payload[5];
    r->arg        = get_u16(&payload[6]);
}

/* ------------------------------------------------------ producers ---- */

static void q_lock(hk_storage_t *s)
{
    if (s->lock != NULL) {
        s->lock(s->lock_arg);
    }
}

static void q_unlock(hk_storage_t *s)
{
    if (s->unlock != NULL) {
        s->unlock(s->lock_arg);
    }
}

/* Push a complete frame atomically; drop the record if it does not fit. */
static void push_frame(hk_storage_t *s, const uint8_t *frame, size_t len)
{
    if (len == 0) {
        return;
    }
    q_lock(s);
    if (hk_ringbuf_free(&s->queue) >= len) {
        (void)hk_ringbuf_write(&s->queue, frame, len);
    } else {
        s->dropped++;
    }
    q_unlock(s);
}

void hk_storage_push_env(hk_storage_t *s, const hk_rec_env_t *r)
{
    uint8_t f[HK_LOG_MAX_FRAME];
    push_frame(s, f, hk_log_frame_env(f, r));
}

void hk_storage_push_imu(hk_storage_t *s, const hk_rec_imu_t *r)
{
    uint8_t decim = (s->cfg.imu_decim == 0) ? 1u : s->cfg.imu_decim;
    if (++s->imu_skip < decim) {
        return;
    }
    s->imu_skip = 0;
    uint8_t f[HK_LOG_MAX_FRAME];
    push_frame(s, f, hk_log_frame_imu(f, r));
}

void hk_storage_push_gps(hk_storage_t *s, const hk_rec_gps_t *r)
{
    uint8_t f[HK_LOG_MAX_FRAME];
    push_frame(s, f, hk_log_frame_gps(f, r));
}

void hk_storage_push_event(hk_storage_t *s, const hk_rec_event_t *r)
{
    uint8_t f[HK_LOG_MAX_FRAME];
    push_frame(s, f, hk_log_frame_event(f, r));
}

/* ------------------------------------------------------ consumer ---- */

hk_status_t hk_storage_init(hk_storage_t *s, const hk_storage_cfg_t *cfg,
                            uint8_t *queue_mem, size_t queue_capacity)
{
    if (s == NULL || queue_mem == NULL) {
        return HK_ERR_PARAM;
    }
    memset(s, 0, sizeof(*s));
    if (cfg != NULL) {
        s->cfg = *cfg;
    }
    if (s->cfg.sync_period_ms == 0)  { s->cfg.sync_period_ms  = 1000u; }
    if (s->cfg.retry_period_ms == 0) { s->cfg.retry_period_ms = 5000u; }
    if (s->cfg.imu_decim == 0)       { s->cfg.imu_decim       = 4u;    }

    if (!hk_ringbuf_init(&s->queue, queue_mem, queue_capacity)) {
        return HK_ERR_PARAM;
    }
    return HK_OK;
}

void hk_storage_set_lock(hk_storage_t *s, void (*lock)(void *),
                         void (*unlock)(void *), void *arg)
{
    s->lock     = lock;
    s->unlock   = unlock;
    s->lock_arg = arg;
}

/* Parse "FL_NNNN.xxx" -> NNNN, or 0 if the name doesn't match. */
static uint16_t parse_session_name(const char *name)
{
    if (name[0] != 'F' || name[1] != 'L' || name[2] != '_') {
        return 0;
    }
    unsigned idx = 0;
    int i = 3;
    for (; i < 7; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return 0;
        }
        idx = idx * 10u + (unsigned)(name[i] - '0');
    }
    return (name[i] == '.') ? (uint16_t)idx : 0;
}

/* Scan /LOGS for the highest FL_NNNN.* index. */
static uint16_t next_session_index(void)
{
    DIR      dir;
    FILINFO  fi;
    uint16_t max_idx = 0;

    if (f_opendir(&dir, "/LOGS") != FR_OK) {
        return 1;
    }
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0] != '\0') {
        uint16_t idx = parse_session_name(fi.fname);
        if (idx > max_idx && idx < 9999u) {
            max_idx = idx;
        }
    }
    (void)f_closedir(&dir);
    return (uint16_t)(max_idx + 1u);
}

static void fs_failed(hk_storage_t *s)
{
    s->write_errors++;
    if (s->files_open) {
        (void)f_close(&s->f_bin);
        (void)f_close(&s->f_csv);
        s->files_open = false;
    }
    (void)f_unmount("");
    s->mounted = false;
}

static bool try_mount(hk_storage_t *s, uint32_t now_ms)
{
    FRESULT mr = f_mount(&s->fs, "", 1);
    s->last_mount_err = (uint8_t)mr;   /* kept so the caller can report WHY */
    if (mr != FR_OK) {
        return false;
    }
    s->mounted = true;

    FRESULT fr = f_mkdir("/LOGS");
    if (fr != FR_OK && fr != FR_EXIST) {
        fs_failed(s);
        return false;
    }

    s->session = next_session_index();
    char path[24];
    (void)snprintf(path, sizeof(path), "/LOGS/FL_%04u.BIN", (unsigned)s->session);
    if (f_open(&s->f_bin, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        fs_failed(s);
        return false;
    }
    (void)snprintf(path, sizeof(path), "/LOGS/FL_%04u.CSV", (unsigned)s->session);
    if (f_open(&s->f_csv, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        (void)f_close(&s->f_bin);
        fs_failed(s);
        return false;
    }
    s->files_open = true;

    static const char header[] =
        "t_ms,state,press_pa,alt_m,t_bmp_c,t_sht1_c,rh1,t_sht2_c,rh2,vbat_v,soc\n";
    UINT bw = 0;
    (void)f_write(&s->f_csv, header, (UINT)(sizeof(header) - 1u), &bw);

    /* session start marker */
    hk_rec_meta_t meta = {
        .t_ms         = now_ms,
        .fw_version   = s->cfg.fw_version,
        .reset_reason = s->cfg.reset_reason,
        .log_version  = HK_LOG_VERSION,
    };
    uint8_t frame[HK_LOG_MAX_FRAME];
    size_t  len = hk_log_frame_meta(frame, &meta);
    if (f_write(&s->f_bin, frame, (UINT)len, &bw) != FR_OK || bw != len) {
        fs_failed(s);
        return false;
    }
    (void)f_sync(&s->f_bin);
    (void)f_sync(&s->f_csv);
    s->last_sync_ms = now_ms;
    return true;
}

static bool write_csv_env(hk_storage_t *s, const hk_rec_env_t *r)
{
    char line[192];
    int  n = snprintf(line, sizeof(line),
                      "%lu,%s,%.1f,%.1f,%.2f,%.2f,%.1f,%.2f,%.1f,%.2f,%.2f\n",
                      (unsigned long)r->t_ms,
                      hk_mission_state_str((hk_mission_state_t)r->mission_state),
                      (double)r->press_pa, (double)r->alt_m,
                      (double)r->temp_bmp_c,
                      (double)r->temp_sht1_c, (double)r->rh_sht1,
                      (double)r->temp_sht2_c, (double)r->rh_sht2,
                      (double)r->vbat_v, (double)r->soc);
    if (n <= 0) {
        return true;   /* formatting problem is not a filesystem failure */
    }
    UINT bw = 0;
    return f_write(&s->f_csv, line, (UINT)n, &bw) == FR_OK && bw == (UINT)n;
}

static bool write_csv_event(hk_storage_t *s, const hk_rec_event_t *r)
{
    char line[96];
    int  n = snprintf(line, sizeof(line), "%lu,EVENT,%s->%s,%u\n",
                      (unsigned long)r->t_ms,
                      hk_mission_state_str((hk_mission_state_t)r->from_state),
                      hk_mission_state_str((hk_mission_state_t)r->to_state),
                      (unsigned)r->arg);
    if (n <= 0) {
        return true;
    }
    UINT bw = 0;
    return f_write(&s->f_csv, line, (UINT)n, &bw) == FR_OK && bw == (UINT)n;
}

/* Drain complete frames from the queue into the session files.
 * Returns true if an EVENT record was written (forces an immediate sync). */
static bool drain_queue(hk_storage_t *s)
{
    bool sync_now = false;
    uint8_t frame[HK_LOG_MAX_FRAME];

    /* bounded work per service tick */
    for (int budget = 0; budget < 64; ++budget) {
        q_lock(s);
        size_t avail = hk_ringbuf_count(&s->queue);
        q_unlock(s);
        if (avail < 7u) {
            break;
        }

        /* peek the 5-byte header without consuming */
        q_lock(s);
        uint8_t hdr[5];
        (void)hk_ringbuf_read(&s->queue, hdr, 5);
        if (hdr[0] != HK_LOG_MAGIC0 || hdr[1] != HK_LOG_MAGIC1) {
            /* should not happen (frames are pushed atomically); resync */
            q_unlock(s);
            continue;
        }
        size_t payload_len = hdr[4];
        size_t rest = payload_len + 2u;
        memcpy(frame, hdr, 5);
        size_t got = hk_ringbuf_read(&s->queue, &frame[5], rest);
        q_unlock(s);
        if (got != rest) {
            break;   /* truncated push mid-flight: impossible by design */
        }
        size_t total = 5u + rest;

        UINT bw = 0;
        if (f_write(&s->f_bin, frame, (UINT)total, &bw) != FR_OK || bw != total) {
            fs_failed(s);
            return false;
        }
        s->records_written++;

        bool csv_ok = true;
        if (frame[3] == HK_REC_ENV) {
            hk_rec_env_t r;
            hk_log_decode_env(&frame[5], &r);
            csv_ok = write_csv_env(s, &r);
        } else if (frame[3] == HK_REC_EVENT) {
            hk_rec_event_t r;
            hk_log_decode_event(&frame[5], &r);
            csv_ok = write_csv_event(s, &r);
            sync_now = true;
        }
        if (!csv_ok) {
            fs_failed(s);
            return false;
        }
    }
    return sync_now;
}

void hk_storage_service(hk_storage_t *s, uint32_t now_ms)
{
    if (s == NULL) {
        return;
    }

    if (!hk_storage_mounted(s)) {
        if (s->first_try_done &&
            (now_ms - s->last_try_ms) < s->cfg.retry_period_ms) {
            return;
        }
        s->last_try_ms    = now_ms;
        s->first_try_done = true;
        if (!try_mount(s, now_ms)) {
            return;
        }
    }

    bool event_written = drain_queue(s);
    if (!hk_storage_mounted(s)) {
        return;   /* drain hit a write error */
    }

    if (event_written ||
        (now_ms - s->last_sync_ms) >= s->cfg.sync_period_ms) {
        if (f_sync(&s->f_bin) != FR_OK || f_sync(&s->f_csv) != FR_OK) {
            fs_failed(s);
            return;
        }
        s->last_sync_ms = now_ms;
    }
}

void hk_storage_close(hk_storage_t *s)
{
    if (s->files_open) {
        (void)f_sync(&s->f_bin);
        (void)f_sync(&s->f_csv);
        (void)f_close(&s->f_bin);
        (void)f_close(&s->f_csv);
        s->files_open = false;
    }
    if (s->mounted) {
        (void)f_unmount("");
        s->mounted = false;
    }
}
