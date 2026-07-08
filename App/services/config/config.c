#include "services/config/config.h"

#include <stdlib.h>
#include <string.h>

#include "common/crc.h"

void hk_config_defaults(hk_config_body_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->mission                = hk_mission_default_cfg();
    cfg->bat_divider_ratio      = 11.0f;    /* R3 100k / R22 10k */
    cfg->storage_sync_period_ms = 1000;
    cfg->imu_log_decim          = 4;
}

/* ------------------------------------------------------------- NV I/O ---- */

static uint32_t rec_crc(const hk_config_rec_t *r)
{
    return hk_crc32((const uint8_t *)r, offsetof(hk_config_rec_t, crc32));
}

static bool rec_valid(const hk_config_rec_t *r)
{
    return r->magic == HK_CONFIG_MAGIC &&
           r->version == HK_CONFIG_VERSION &&
           r->body_size == (uint16_t)sizeof(hk_config_body_t) &&
           r->crc32 == rec_crc(r);
}

static size_t slot_count(const hk_nv_t *nv)
{
    return nv->size / HK_CONFIG_SLOT;
}

/* Scan every slot; return the newest valid record (by seq). */
static bool find_newest(const hk_nv_t *nv, hk_config_rec_t *best,
                        size_t *best_slot)
{
    bool found = false;
    for (size_t i = 0; i < slot_count(nv); ++i) {
        hk_config_rec_t r;
        if (hk_nv_read(nv, i * HK_CONFIG_SLOT, &r, sizeof(r)) != HK_OK) {
            continue;
        }
        if (rec_valid(&r) && (!found || r.seq > best->seq)) {
            *best = r;
            if (best_slot != NULL) {
                *best_slot = i;
            }
            found = true;
        }
    }
    return found;
}

uint32_t hk_config_load(const hk_nv_t *nv, hk_config_body_t *cfg)
{
    hk_config_rec_t best;
    if (nv != NULL && find_newest(nv, &best, NULL)) {
        *cfg = best.body;
        return best.seq;
    }
    hk_config_defaults(cfg);
    return 0;
}

/* First slot still in erased state (all header bytes 0xFF). */
static bool slot_is_free(const hk_nv_t *nv, size_t slot)
{
    uint8_t hdr[8];
    if (hk_nv_read(nv, slot * HK_CONFIG_SLOT, hdr, sizeof(hdr)) != HK_OK) {
        return false;
    }
    for (size_t i = 0; i < sizeof(hdr); ++i) {
        if (hdr[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

hk_status_t hk_config_save(const hk_nv_t *nv, const hk_config_body_t *cfg)
{
    if (nv == NULL || cfg == NULL || slot_count(nv) == 0) {
        return HK_ERR_PARAM;
    }

    hk_config_rec_t newest;
    uint32_t next_seq = 1;
    if (find_newest(nv, &newest, NULL)) {
        next_seq = newest.seq + 1u;
    }

    size_t slot = slot_count(nv);
    for (size_t i = 0; i < slot_count(nv); ++i) {
        if (slot_is_free(nv, i)) {
            slot = i;
            break;
        }
    }
    if (slot == slot_count(nv)) {
        /* journal full: erase and start over (rare; config saves are rare) */
        hk_status_t st = hk_nv_erase_all(nv);
        if (st != HK_OK) {
            return st;
        }
        slot = 0;
    }

    hk_config_rec_t rec;
    memset(&rec, 0xFF, sizeof(rec));   /* pad bytes stay in erased state */
    rec.magic     = HK_CONFIG_MAGIC;
    rec.version   = HK_CONFIG_VERSION;
    rec.body_size = (uint16_t)sizeof(hk_config_body_t);
    rec.seq       = next_seq;
    rec.body      = *cfg;
    rec.crc32     = rec_crc(&rec);

    hk_status_t st = hk_nv_write(nv, slot * HK_CONFIG_SLOT, &rec, sizeof(rec));
    if (st != HK_OK) {
        return st;
    }

    hk_config_rec_t verify;
    st = hk_nv_read(nv, slot * HK_CONFIG_SLOT, &verify, sizeof(verify));
    if (st != HK_OK) {
        return st;
    }
    return (rec_valid(&verify) && verify.seq == next_seq) ? HK_OK : HK_ERR_CRC;
}

/* ------------------------------------------------------------ INI text ---- */

typedef enum { KV_F32, KV_U32, KV_U8 } kv_type_t;

typedef struct {
    const char *key;
    kv_type_t   type;
    size_t      offset;   /* into hk_config_body_t */
} kv_entry_t;

#define KV(name, type, field) { name, type, offsetof(hk_config_body_t, field) }

static const kv_entry_t k_keys[] = {
    KV("selftest_timeout_ms",   KV_U32, mission.selftest_timeout_ms),
    KV("arm_mode",              KV_U8,  mission.arm_mode),
    KV("arm_altitude_m",        KV_F32, mission.arm_altitude_m),
    KV("arm_hold_ms",           KV_U32, mission.arm_hold_ms),
    KV("release_freefall_g",    KV_F32, mission.release_freefall_g),
    KV("release_hold_ms",       KV_U32, mission.release_hold_ms),
    KV("release_vspeed_ms",     KV_F32, mission.release_vspeed_ms),
    KV("release_actuation_ms",  KV_U32, mission.release_actuation_ms),
    KV("landed_vspeed_ms",      KV_F32, mission.landed_vspeed_ms),
    KV("landed_accel_tol_g",    KV_F32, mission.landed_accel_tol_g),
    KV("landed_hold_ms",        KV_U32, mission.landed_hold_ms),
    KV("descent_timeout_ms",    KV_U32, mission.descent_timeout_ms),
    KV("recovery_after_ms",     KV_U32, mission.recovery_after_ms),
    KV("servo_hold_deg",        KV_F32, mission.servo_hold_deg),
    KV("servo_release_deg",     KV_F32, mission.servo_release_deg),
    KV("bat_divider_ratio",     KV_F32, bat_divider_ratio),
    KV("storage_sync_period_ms",KV_U32, storage_sync_period_ms),
    KV("imu_log_decim",         KV_U8,  imu_log_decim),
};

bool hk_config_apply_kv(hk_config_body_t *cfg, const char *key,
                        const char *value)
{
    for (size_t i = 0; i < sizeof(k_keys) / sizeof(k_keys[0]); ++i) {
        if (strcmp(key, k_keys[i].key) != 0) {
            continue;
        }
        uint8_t *dst = (uint8_t *)cfg + k_keys[i].offset;
        char *end = NULL;
        switch (k_keys[i].type) {
        case KV_F32: {
            float v = strtof(value, &end);
            if (end == value) { return false; }
            memcpy(dst, &v, sizeof(v));
            break;
        }
        case KV_U32: {
            unsigned long v = strtoul(value, &end, 0);
            if (end == value) { return false; }
            uint32_t v32 = (uint32_t)v;
            memcpy(dst, &v32, sizeof(v32));
            break;
        }
        case KV_U8: {
            unsigned long v = strtoul(value, &end, 0);
            if (end == value || v > 255u) { return false; }
            *dst = (uint8_t)v;
            break;
        }
        default:
            return false;
        }
        return true;
    }
    return false;
}

static const char *skip_ws(const char *p, const char *lim)
{
    while (p < lim && (*p == ' ' || *p == '\t' || *p == '\r')) { ++p; }
    return p;
}

int hk_config_apply_ini(hk_config_body_t *cfg, const char *text, size_t len)
{
    int applied = 0;
    const char *p   = text;
    const char *lim = text + len;

    while (p < lim) {
        const char *nl = memchr(p, '\n', (size_t)(lim - p));
        const char *line_end = (nl != NULL) ? nl : lim;

        const char *s = skip_ws(p, line_end);
        if (s < line_end && *s != '#' && *s != ';') {
            const char *eq = memchr(s, '=', (size_t)(line_end - s));
            if (eq != NULL) {
                /* trim key */
                const char *ke = eq;
                while (ke > s && (ke[-1] == ' ' || ke[-1] == '\t')) { --ke; }
                /* trim value */
                const char *v  = skip_ws(eq + 1, line_end);
                const char *ve = line_end;
                while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t' ||
                                  ve[-1] == '\r')) { --ve; }

                char key[48], val[48];
                size_t kl = (size_t)(ke - s);
                size_t vl = (size_t)(ve - v);
                if (kl > 0 && kl < sizeof(key) && vl > 0 && vl < sizeof(val)) {
                    memcpy(key, s, kl); key[kl] = '\0';
                    memcpy(val, v, vl); val[vl] = '\0';
                    if (hk_config_apply_kv(cfg, key, val)) {
                        ++applied;
                    }
                }
            }
        }
        if (nl == NULL) { break; }
        p = nl + 1;
    }
    return applied;
}
