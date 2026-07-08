#ifndef HK_SD_SPI_H
#define HK_SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "bus/spi_if.h"
#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SD/SDHC card driver over SPI (J3 header module). Pure protocol logic on
 * top of hk_spi_bus_t: host-testable against a mocked/behavioral SPI.
 *
 * Init sequence: 80 idle clocks -> CMD0 -> CMD8 (v2 detect) -> ACMD41 loop
 * (HCS) -> CMD58 (OCR/CCS) -> CMD16(512) for SDSC -> fast clock.
 * Data: single-block CMD17/CMD24 with CRC16 verification both directions.
 *
 * A missing card fails CMD0 with HK_ERR_NOT_FOUND so callers can degrade
 * gracefully and retry later.
 */

#define HK_SD_BLOCK_SIZE 512u

typedef struct {
    const hk_spi_bus_t *spi;
    uint32_t data_speed_hz;   /* clock after init (e.g. 10500000)  */
    bool     sdhc;            /* true: LBA addressing (SDHC/SDXC)  */
    bool     v2;              /* CMD8 accepted                     */
    bool     ready;
} hk_sd_t;

/* Full power-on init. data_speed_hz == 0 defaults to 10.5 MHz. */
hk_status_t hk_sd_init(hk_sd_t *sd, const hk_spi_bus_t *spi,
                       uint32_t data_speed_hz);

/* Read one 512-byte block; CRC16-verified (HK_ERR_CRC on mismatch). */
hk_status_t hk_sd_read_block(hk_sd_t *sd, uint32_t lba,
                             uint8_t buf[HK_SD_BLOCK_SIZE]);

/* Write one 512-byte block; waits for the card to accept and finish. */
hk_status_t hk_sd_write_block(hk_sd_t *sd, uint32_t lba,
                              const uint8_t buf[HK_SD_BLOCK_SIZE]);

/* Wait until the card is not busy (flush after a write burst). */
hk_status_t hk_sd_wait_ready(hk_sd_t *sd);

#ifdef __cplusplus
}
#endif

#endif /* HK_SD_SPI_H */
