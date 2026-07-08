#ifndef HK_DISKIO_SD_H
#define HK_DISKIO_SD_H

#include "drivers/sd_spi/sd_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Binds the SD card instance to the FatFs diskio layer (diskio_sd.c).
 * Call before the first f_mount. The card does not need to be initialized
 * yet: disk_initialize() runs hk_sd_init on demand, so a card inserted
 * after boot is picked up by the storage service's retry loop.
 */
void hk_diskio_sd_bind(hk_sd_t *sd);

#ifdef __cplusplus
}
#endif

#endif /* HK_DISKIO_SD_H */
