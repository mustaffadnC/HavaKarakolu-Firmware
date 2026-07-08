#ifndef HK_CONFIG_H
#define HK_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus/nv_if.h"
#include "common/status.h"
#include "services/mission/mission.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Field-tunable parameters, persisted two ways:
 *
 *  1) Internal-flash journal (bus/nv_if): records are APPENDED at a fixed
 *     stride into one erase sector; the newest CRC-valid record wins. A torn
 *     write invalidates only that record, so the previous config survives
 *     power loss. Erase happens only when the journal is full (~hundreds of
 *     saves on a 128 KB sector).
 *
 *  2) SD card "/CONFIG.INI" overlay ("key = value" lines): applied on top of
 *     the NV values at boot. This is the no-debugger tuning path -- crucial
 *     while the SWD routing question is open (docs/ee-questions.md S1).
 */

#define HK_CONFIG_MAGIC     0x46434B48u   /* "HKCF" little-endian */
#define HK_CONFIG_VERSION   1u
#define HK_CONFIG_SLOT      512u          /* journal stride (>= record size) */

/* The tunable payload. Extend ONLY by appending + bumping HK_CONFIG_VERSION. */
typedef struct {
    hk_mission_cfg_t mission;
    float            bat_divider_ratio;
    uint32_t         storage_sync_period_ms;
    uint8_t          imu_log_decim;
    uint8_t          reserved[3];
} hk_config_body_t;

/* On-NV record: header + body + CRC32 over everything before the CRC. */
typedef struct {
    uint32_t         magic;
    uint16_t         version;
    uint16_t         body_size;
    uint32_t         seq;
    hk_config_body_t body;
    uint32_t         crc32;
} hk_config_rec_t;

/* Compile-time defaults (mission defaults + board values). */
void hk_config_defaults(hk_config_body_t *cfg);

/* Load the newest valid record; falls back to defaults when none is found.
 * Returns the record's seq, or 0 if defaults were used. */
uint32_t hk_config_load(const hk_nv_t *nv, hk_config_body_t *cfg);

/* Append a new record (seq = newest + 1). Erases the journal only when it
 * is full. Verifies by read-back. */
hk_status_t hk_config_save(const hk_nv_t *nv, const hk_config_body_t *cfg);

/* Apply one "key" = "value" pair; returns true if the key is known. */
bool hk_config_apply_kv(hk_config_body_t *cfg, const char *key,
                        const char *value);

/* Apply a whole INI text ('#'/';' comments, whitespace tolerated).
 * Returns the number of keys applied. */
int hk_config_apply_ini(hk_config_body_t *cfg, const char *text, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HK_CONFIG_H */
