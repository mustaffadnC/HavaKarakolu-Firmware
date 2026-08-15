/*
 * FatFs diskio glue -> hk_sd_spi driver (single volume, pdrv 0).
 *
 * Target builds link THIS file; host tests link tests/host/mocks/diskio_ram.c
 * instead (same diskio.h contract, RAM-backed).
 */
#include <stddef.h>

#include "ff.h"       /* BYTE/UINT/LBA_t; must precede diskio.h */
#include "diskio.h"
#include "drivers/sd_spi/sd_spi.h"

static hk_sd_t *s_sd;

/* Bind the initialized (or to-be-initialized) card before f_mount. */
void hk_diskio_sd_bind(hk_sd_t *sd)
{
    s_sd = sd;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0 || s_sd == NULL) {
        return STA_NOINIT;
    }
    return s_sd->ready ? 0 : STA_NOINIT;
}

/*
 * A failed transfer clears sd->ready so this re-runs the card init sequence.
 * Without that the storage service could never recover: a single write error
 * unmounts the volume, but the card object still looked ready, so every retry
 * skipped hk_sd_init() and f_mount kept returning FR_DISK_ERR forever.
 * Observed on hardware 2026-08-15 -- logging stopped after 4457 records and
 * never came back. See docs/DEVIR-TESLIM.md 8.8.
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0 || s_sd == NULL) {
        return STA_NOINIT;
    }
    if (!s_sd->ready) {
        if (hk_sd_init(s_sd, s_sd->spi, s_sd->data_speed_hz) != HK_OK) {
            return STA_NOINIT;
        }
    }
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || s_sd == NULL || !s_sd->ready) {
        return RES_NOTRDY;
    }
    for (UINT i = 0; i < count; ++i) {
        if (hk_sd_read_block(s_sd, (uint32_t)sector + i,
                             buff + (size_t)i * HK_SD_BLOCK_SIZE) != HK_OK) {
            s_sd->ready = false;   /* force a card re-init on the next mount */
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || s_sd == NULL || !s_sd->ready) {
        return RES_NOTRDY;
    }
    for (UINT i = 0; i < count; ++i) {
        if (hk_sd_write_block(s_sd, (uint32_t)sector + i,
                              buff + (size_t)i * HK_SD_BLOCK_SIZE) != HK_OK) {
            s_sd->ready = false;   /* force a card re-init on the next mount */
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0 || s_sd == NULL) {
        return RES_NOTRDY;
    }
    switch (cmd) {
    case CTRL_SYNC:
        return (hk_sd_wait_ready(s_sd) == HK_OK) ? RES_OK : RES_ERROR;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = HK_SD_BLOCK_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;   /* erase block unknown: 1 sector */
        return RES_OK;
    case GET_SECTOR_COUNT:
        /* only needed by f_mkfs, which is never used on target */
        return RES_PARERR;
    default:
        return RES_PARERR;
    }
}
