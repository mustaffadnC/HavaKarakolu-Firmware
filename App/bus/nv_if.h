#ifndef HK_NV_IF_H
#define HK_NV_IF_H

#include <stddef.h>
#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal non-volatile storage interface for the config service.
 * Implementations:
 *   - bsp/nv_flash_stm32.c : STM32F405 internal flash, sector 11 (128 KB at
 *     0x080E0000; linker must cap FLASH at 896K so code never lands there)
 *   - host tests           : RAM-backed mock with fault injection
 *
 * Flash semantics: write() may only clear bits (erased = 0xFF); call
 * erase_all() before rewriting a slot region. The config service's
 * double-slot scheme is built on these rules.
 */
typedef struct hk_nv hk_nv_t;

struct hk_nv {
    void  *ctx;
    size_t size;   /* usable bytes */

    hk_status_t (*read)(void *ctx, size_t offset, void *buf, size_t len);
    hk_status_t (*write)(void *ctx, size_t offset, const void *buf, size_t len);
    hk_status_t (*erase_all)(void *ctx);
};

static inline hk_status_t hk_nv_read(const hk_nv_t *nv, size_t off,
                                     void *buf, size_t len)
{
    return nv->read(nv->ctx, off, buf, len);
}

static inline hk_status_t hk_nv_write(const hk_nv_t *nv, size_t off,
                                      const void *buf, size_t len)
{
    return nv->write(nv->ctx, off, buf, len);
}

static inline hk_status_t hk_nv_erase_all(const hk_nv_t *nv)
{
    return nv->erase_all(nv->ctx);
}

#if !defined(HK_HOST)
/* STM32 internal-flash implementation (bsp/nv_flash_stm32.c). */
hk_status_t hk_nv_flash_init(hk_nv_t *nv);
#endif

#ifdef __cplusplus
}
#endif

#endif /* HK_NV_IF_H */
