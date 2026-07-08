#ifndef HK_STORAGE_H
#define HK_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/ringbuf.h"
#include "common/status.h"
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SD-card flight logging service (the board's ONLY data output).
 *
 * Producer/consumer split keeps it RTOS-free and host-testable:
 *   - hk_storage_push_*()  : called from any task; serializes the record
 *     into a byte queue under the caller-provided lock hooks.
 *   - hk_storage_service() : called ONLY from the storage task; owns every
 *     FatFs call (mount, session files, drain queue, periodic f_sync).
 *
 * Robustness rules:
 *   - never auto-formats the card (a flaky bus must not erase a flight log)
 *   - mount failure => degraded mode; retries every retry_period_ms
 *   - f_sync both files every sync_period_ms AND right after every EVENT,
 *     bounding power-loss data loss to one sync period
 *   - binary frames carry magic + CRC16 so a reader can resync past a torn
 *     tail (tools/hk_log_reader.py)
 *
 * Session layout on card:  /LOGS/FL_0001.BIN  (framed binary, authoritative)
 *                          /LOGS/FL_0001.CSV  (human quick-look: env + events)
 */

/* ---- binary frame: 'H' 'K' ver type len payload crc16(ver..payload) ---- */
#define HK_LOG_MAGIC0      0x48u   /* 'H' */
#define HK_LOG_MAGIC1      0x4Bu   /* 'K' */
#define HK_LOG_VERSION     1u

typedef enum {
    HK_REC_META  = 1,
    HK_REC_ENV   = 2,
    HK_REC_IMU   = 3,
    HK_REC_GPS   = 4,
    HK_REC_EVENT = 5
} hk_record_type_t;

/* ---- record payloads (serialized little-endian, floats as IEEE754) ---- */

typedef struct {
    uint32_t t_ms;
    uint16_t fw_version;     /* 0xMMmm */
    uint8_t  reset_reason;
    uint8_t  log_version;
} hk_rec_meta_t;

typedef struct {
    uint32_t t_ms;
    uint8_t  mission_state;
    float    press_pa, alt_m, temp_bmp_c;
    float    temp_sht1_c, rh_sht1, temp_sht2_c, rh_sht2;
    float    vbat_v, soc;
} hk_rec_env_t;

typedef struct {
    uint32_t t_ms;
    float    ax, ay, az;      /* m/s^2 */
    float    gx, gy, gz;      /* rad/s */
    float    roll_deg, pitch_deg;
} hk_rec_imu_t;

typedef struct {
    uint32_t t_ms;
    double   lat_deg, lon_deg;
    float    alt_m, speed_mps, course_deg;
    uint8_t  satellites, fix_quality, valid;
} hk_rec_gps_t;

typedef struct {
    uint32_t t_ms;
    uint8_t  from_state, to_state;
    uint16_t arg;
} hk_rec_event_t;

/* ---- configuration ---- */

typedef struct {
    uint32_t sync_period_ms;    /* 0 -> 1000  */
    uint32_t retry_period_ms;   /* 0 -> 5000  */
    uint8_t  imu_decim;         /* keep 1-of-N pushed IMU records; 0 -> 4 */
    uint16_t fw_version;        /* stamped into the META record */
    uint8_t  reset_reason;
} hk_storage_cfg_t;

/* ---- service state ---- */

typedef struct {
    hk_storage_cfg_t cfg;
    hk_ringbuf_t     queue;

    /* lock hooks guarding queue writes from multiple producer tasks */
    void (*lock)(void *arg);
    void (*unlock)(void *arg);
    void *lock_arg;

    /* writer-side (storage task only) */
    FATFS    fs;
    FIL      f_bin, f_csv;
    bool     mounted;
    bool     files_open;
    uint16_t session;
    uint32_t last_sync_ms, last_try_ms;
    bool     first_try_done;
    uint8_t  imu_skip;

    /* stats (best effort, informational) */
    uint32_t dropped;         /* records lost to a full queue      */
    uint32_t write_errors;
    uint32_t records_written;
} hk_storage_t;

/* No filesystem access here: wires the queue (capacity power of two). */
hk_status_t hk_storage_init(hk_storage_t *s, const hk_storage_cfg_t *cfg,
                            uint8_t *queue_mem, size_t queue_capacity);

/* Optional producer-side lock (FreeRTOS critical section on target). */
void hk_storage_set_lock(hk_storage_t *s, void (*lock)(void *),
                         void (*unlock)(void *), void *arg);

/* ---- producers (any task) ---- */
void hk_storage_push_env(hk_storage_t *s, const hk_rec_env_t *r);
void hk_storage_push_imu(hk_storage_t *s, const hk_rec_imu_t *r);   /* decimated */
void hk_storage_push_gps(hk_storage_t *s, const hk_rec_gps_t *r);
void hk_storage_push_event(hk_storage_t *s, const hk_rec_event_t *r);

/* ---- consumer (storage task ONLY) ----
 * Mount/open/retry + drain + periodic sync. Call every ~100 ms. */
void hk_storage_service(hk_storage_t *s, uint32_t now_ms);

/* Close files cleanly (landed/shutdown path). Remount happens via service. */
void hk_storage_close(hk_storage_t *s);

static inline bool hk_storage_mounted(const hk_storage_t *s)
{
    return s->mounted && s->files_open;
}

/* ---- frame helpers (shared with tests / reader tooling) ---- */

/* Serialize one record into out (>= HK_LOG_MAX_FRAME bytes).
 * Returns the frame length, or 0 on bad type/params. */
#define HK_LOG_MAX_FRAME 64u
size_t hk_log_frame_meta(uint8_t *out, const hk_rec_meta_t *r);
size_t hk_log_frame_env(uint8_t *out, const hk_rec_env_t *r);
size_t hk_log_frame_imu(uint8_t *out, const hk_rec_imu_t *r);
size_t hk_log_frame_gps(uint8_t *out, const hk_rec_gps_t *r);
size_t hk_log_frame_event(uint8_t *out, const hk_rec_event_t *r);

/* Parse/verify one frame at buf (len bytes available). Returns frame length
 * and sets *type, or 0 if incomplete/invalid (caller resyncs on magic). */
size_t hk_log_frame_parse(const uint8_t *buf, size_t len, uint8_t *type);

/* Payload decoders for verified frames (used by tests). */
void hk_log_decode_env(const uint8_t *payload, hk_rec_env_t *r);
void hk_log_decode_event(const uint8_t *payload, hk_rec_event_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HK_STORAGE_H */
