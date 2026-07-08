#ifndef HK_DISKIO_RAM_H
#define HK_DISKIO_RAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RAM disk control for host tests (see diskio_ram.c). */
bool   hk_ramdisk_create(uint32_t sectors);
void   hk_ramdisk_destroy(void);
size_t hk_ramdisk_size(void);
void   hk_ramdisk_snapshot(uint8_t *dst);
void   hk_ramdisk_restore(const uint8_t *src);
/* 0: fail every write from now on; N>0: fail after N more writes; -1: off. */
void   hk_ramdisk_fail_after(int writes);

#ifdef __cplusplus
}
#endif

#endif /* HK_DISKIO_RAM_H */
