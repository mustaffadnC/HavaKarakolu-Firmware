/*
 * RAM-backed FatFs diskio for host tests (pdrv 0).
 *
 * Extras for robustness testing:
 *   - snapshot/restore: emulate power loss between two syncs
 *   - fail injection: force write failures after N writes (degraded mode)
 */
#include "mocks/diskio_ram.h"

#include <stdlib.h>
#include <string.h>

#include "ff.h"       /* BYTE/UINT/LBA_t; must precede diskio.h */
#include "diskio.h"

#define SS 512u

static uint8_t *s_disk;
static LBA_t    s_sectors;
static int      s_fail_after_writes = -1;   /* -1: never fail */

bool hk_ramdisk_create(uint32_t sectors)
{
    free(s_disk);
    s_disk = (uint8_t *)calloc(sectors, SS);
    s_sectors = sectors;
    s_fail_after_writes = -1;
    return s_disk != NULL;
}

void hk_ramdisk_destroy(void)
{
    free(s_disk);
    s_disk = NULL;
    s_sectors = 0;
}

size_t hk_ramdisk_size(void)
{
    return (size_t)s_sectors * SS;
}

/* Copy the current disk image (caller owns the buffer of hk_ramdisk_size()). */
void hk_ramdisk_snapshot(uint8_t *dst)
{
    memcpy(dst, s_disk, hk_ramdisk_size());
}

/* Restore a snapshot -- "the power came back": everything written after the
 * snapshot is gone. */
void hk_ramdisk_restore(const uint8_t *src)
{
    memcpy(s_disk, src, hk_ramdisk_size());
}

void hk_ramdisk_fail_after(int writes)
{
    s_fail_after_writes = writes;
}

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0 && s_disk != NULL) ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || s_disk == NULL) {
        return RES_NOTRDY;
    }
    if (sector + count > s_sectors) {
        return RES_PARERR;
    }
    memcpy(buff, s_disk + (size_t)sector * SS, (size_t)count * SS);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || s_disk == NULL) {
        return RES_NOTRDY;
    }
    if (sector + count > s_sectors) {
        return RES_PARERR;
    }
    if (s_fail_after_writes == 0) {
        return RES_ERROR;
    }
    if (s_fail_after_writes > 0) {
        s_fail_after_writes--;
    }
    memcpy(s_disk + (size_t)sector * SS, buff, (size_t)count * SS);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0 || s_disk == NULL) {
        return RES_NOTRDY;
    }
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = s_sectors;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = SS;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
