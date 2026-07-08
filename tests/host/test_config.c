#include "services/config/config.h"
#include "hk_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- RAM-backed hk_nv_t mock with flash semantics + fault injection ---- */

#define NV_SIZE (8u * HK_CONFIG_SLOT)

typedef struct {
    uint8_t mem[NV_SIZE];
    int     fail_writes;   /* -1: never */
    int     erases;
} nv_ram_t;

static hk_status_t ram_read(void *ctx, size_t off, void *buf, size_t len)
{
    nv_ram_t *m = (nv_ram_t *)ctx;
    if (off + len > NV_SIZE) { return HK_ERR_PARAM; }
    memcpy(buf, &m->mem[off], len);
    return HK_OK;
}

static hk_status_t ram_write(void *ctx, size_t off, const void *buf, size_t len)
{
    nv_ram_t *m = (nv_ram_t *)ctx;
    if (off + len > NV_SIZE) { return HK_ERR_PARAM; }
    if (m->fail_writes == 0) { return HK_ERR_IO; }
    if (m->fail_writes > 0) { m->fail_writes--; }
    /* flash semantics: bits can only clear */
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0; i < len; ++i) {
        m->mem[off + i] &= src[i];
    }
    return HK_OK;
}

static hk_status_t ram_erase(void *ctx)
{
    nv_ram_t *m = (nv_ram_t *)ctx;
    memset(m->mem, 0xFF, NV_SIZE);
    m->erases++;
    return HK_OK;
}

static void nv_init(nv_ram_t *m, hk_nv_t *nv)
{
    memset(m->mem, 0xFF, NV_SIZE);
    m->fail_writes = -1;
    m->erases      = 0;
    nv->ctx        = m;
    nv->size       = NV_SIZE;
    nv->read       = ram_read;
    nv->write      = ram_write;
    nv->erase_all  = ram_erase;
}

/* ---- tests ---- */

static void test_defaults_when_empty(void)
{
    nv_ram_t m;
    hk_nv_t  nv;
    nv_init(&m, &nv);

    hk_config_body_t cfg;
    HK_CHECK_EQ_INT((long)hk_config_load(&nv, &cfg), 0L);
    HK_CHECK(fabsf(cfg.bat_divider_ratio - 11.0f) < 1e-6f);
    HK_CHECK(fabsf(cfg.mission.arm_altitude_m - 30.0f) < 1e-6f);
}

static void test_save_load_roundtrip(void)
{
    nv_ram_t m;
    hk_nv_t  nv;
    nv_init(&m, &nv);

    hk_config_body_t cfg;
    hk_config_defaults(&cfg);
    cfg.mission.arm_altitude_m = 45.5f;
    cfg.imu_log_decim          = 8;

    HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);

    hk_config_body_t back;
    HK_CHECK_EQ_INT((long)hk_config_load(&nv, &back), 1L);
    HK_CHECK(fabsf(back.mission.arm_altitude_m - 45.5f) < 1e-6f);
    HK_CHECK_EQ_INT(back.imu_log_decim, 8);

    /* second save appends with seq 2 and wins */
    cfg.mission.arm_altitude_m = 60.0f;
    HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);
    HK_CHECK_EQ_INT((long)hk_config_load(&nv, &back), 2L);
    HK_CHECK(fabsf(back.mission.arm_altitude_m - 60.0f) < 1e-6f);
    HK_CHECK_EQ_INT(m.erases, 0);   /* no erase needed yet */
}

static void test_corrupt_newest_falls_back(void)
{
    nv_ram_t m;
    hk_nv_t  nv;
    nv_init(&m, &nv);

    hk_config_body_t cfg;
    hk_config_defaults(&cfg);
    cfg.mission.arm_altitude_m = 11.0f;
    HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);   /* slot 0, seq 1 */
    cfg.mission.arm_altitude_m = 22.0f;
    HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);   /* slot 1, seq 2 */

    /* corrupt the newest record (slot 1): torn write / bit rot */
    m.mem[HK_CONFIG_SLOT + 20] ^= 0x5Au;

    hk_config_body_t back;
    HK_CHECK_EQ_INT((long)hk_config_load(&nv, &back), 1L);   /* older wins */
    HK_CHECK(fabsf(back.mission.arm_altitude_m - 11.0f) < 1e-6f);
}

static void test_journal_full_erases_and_restarts(void)
{
    nv_ram_t m;
    hk_nv_t  nv;
    nv_init(&m, &nv);

    hk_config_body_t cfg;
    hk_config_defaults(&cfg);

    /* fill all 8 slots, then one more save must erase + restart */
    for (int i = 0; i < 8; ++i) {
        cfg.mission.arm_altitude_m = (float)i;
        HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);
    }
    HK_CHECK_EQ_INT(m.erases, 0);
    cfg.mission.arm_altitude_m = 99.0f;
    HK_CHECK_EQ_INT(hk_config_save(&nv, &cfg), HK_OK);
    HK_CHECK_EQ_INT(m.erases, 1);

    hk_config_body_t back;
    HK_CHECK_EQ_INT((long)hk_config_load(&nv, &back), 9L);  /* seq continues */
    HK_CHECK(fabsf(back.mission.arm_altitude_m - 99.0f) < 1e-6f);
}

static void test_write_failure_reported(void)
{
    nv_ram_t m;
    hk_nv_t  nv;
    nv_init(&m, &nv);
    m.fail_writes = 0;

    hk_config_body_t cfg;
    hk_config_defaults(&cfg);
    HK_CHECK(hk_config_save(&nv, &cfg) != HK_OK);
}

static void test_ini_overlay(void)
{
    hk_config_body_t cfg;
    hk_config_defaults(&cfg);

    const char ini[] =
        "# kapsul saha ayarlari\r\n"
        "arm_altitude_m = 55.5\n"
        "  release_hold_ms=450\t\n"
        "servo_release_deg = 135\n"
        "imu_log_decim = 2\n"
        "; yorum satiri\n"
        "bilinmeyen_anahtar = 42\n"
        "landed_vspeed_ms 0.5\n"          /* missing '=': ignored */
        "arm_mode = 1";                    /* last line without newline */

    int n = hk_config_apply_ini(&cfg, ini, sizeof(ini) - 1);
    HK_CHECK_EQ_INT(n, 5);
    HK_CHECK(fabsf(cfg.mission.arm_altitude_m - 55.5f) < 1e-6f);
    HK_CHECK_EQ_INT((long)cfg.mission.release_hold_ms, 450L);
    HK_CHECK(fabsf(cfg.mission.servo_release_deg - 135.0f) < 1e-6f);
    HK_CHECK_EQ_INT(cfg.imu_log_decim, 2);
    HK_CHECK_EQ_INT(cfg.mission.arm_mode, 1);
    /* untouched key keeps its default */
    HK_CHECK(fabsf(cfg.mission.landed_vspeed_ms - 0.7f) < 1e-6f);
}

static void test_ini_bad_values_rejected(void)
{
    hk_config_body_t cfg;
    hk_config_defaults(&cfg);
    HK_CHECK(!hk_config_apply_kv(&cfg, "arm_altitude_m", "abc"));
    HK_CHECK(!hk_config_apply_kv(&cfg, "imu_log_decim", "999"));
    HK_CHECK(!hk_config_apply_kv(&cfg, "yok_boyle_anahtar", "1"));
    HK_CHECK(hk_config_apply_kv(&cfg, "release_hold_ms", "0x1F4"));
    HK_CHECK_EQ_INT((long)cfg.mission.release_hold_ms, 500L);
}

int main(void)
{
    printf("test_config\n");
    test_defaults_when_empty();
    test_save_load_roundtrip();
    test_corrupt_newest_falls_back();
    test_journal_full_erases_and_restarts();
    test_write_failure_reported();
    test_ini_overlay();
    test_ini_bad_values_rejected();
    return hk_test_summary();
}
