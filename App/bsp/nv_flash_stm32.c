#include "bus/nv_if.h"

#if !defined(HK_HOST)

#include <string.h>

#include "main.h"

/*
 * hk_nv_t backed by STM32F405 internal flash SECTOR 11 (128 KB @ 0x080E0000).
 *
 * REQUIREMENT: the linker script must cap FLASH LENGTH at 896K so no code or
 * rodata is ever placed in this sector (see docs/cubemx-setup.md).
 *
 * Writes are word-aligned; the config service only appends to erased space,
 * so no read-modify-write is needed.
 */

#define NV_BASE     0x080E0000u
#define NV_SIZE     0x20000u
#define NV_SECTOR   FLASH_SECTOR_11

static hk_status_t nv_read(void *ctx, size_t offset, void *buf, size_t len)
{
    (void)ctx;
    if (offset + len > NV_SIZE) {
        return HK_ERR_PARAM;
    }
    memcpy(buf, (const void *)(NV_BASE + offset), len);
    return HK_OK;
}

static hk_status_t nv_write(void *ctx, size_t offset, const void *buf, size_t len)
{
    (void)ctx;
    if (offset + len > NV_SIZE) {
        return HK_ERR_PARAM;
    }

    HAL_FLASH_Unlock();

    hk_status_t   result = HK_OK;
    const uint8_t *src   = (const uint8_t *)buf;
    for (size_t i = 0; i < len && result == HK_OK; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,
                              NV_BASE + offset + i, src[i]) != HAL_OK) {
            result = HK_ERR_IO;
        }
    }

    HAL_FLASH_Lock();
    return result;
}

static hk_status_t nv_erase(void *ctx)
{
    (void)ctx;
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef er = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = NV_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,   /* 2.7-3.6V: word ops */
    };
    uint32_t bad_sector = 0;
    HAL_StatusTypeDef hs = HAL_FLASHEx_Erase(&er, &bad_sector);

    HAL_FLASH_Lock();
    return (hs == HAL_OK) ? HK_OK : HK_ERR_IO;
}

hk_status_t hk_nv_flash_init(hk_nv_t *nv)
{
    if (nv == NULL) {
        return HK_ERR_PARAM;
    }
    nv->ctx       = NULL;
    nv->size      = NV_SIZE;
    nv->read      = nv_read;
    nv->write     = nv_write;
    nv->erase_all = nv_erase;
    return HK_OK;
}

#endif /* !HK_HOST */
