#ifndef HK_SPI_IF_H
#define HK_SPI_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Abstract SPI bus (mirrors bus/i2c_bus_if.h). Concrete implementations:
 *   - spi_hw.c : STM32 HAL SPI1 (blocking transfers, soft chip-select GPIO)
 *   - mock     : host unit tests (scripted byte exchange / SD card model)
 *
 * Full-duplex byte exchange: tx == NULL sends 0xFF fill bytes (SD idle
 * pattern), rx == NULL discards the received bytes.
 */
typedef struct hk_spi_bus hk_spi_bus_t;

struct hk_spi_bus {
    void *ctx;

    /* Exchange `len` bytes. Either tx or rx (not both) may be NULL. */
    hk_status_t (*xfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);

    /* Reconfigure the clock: SD init needs <=400 kHz, data phase runs fast.
       The implementation picks the closest achievable rate <= hz. */
    hk_status_t (*set_speed)(void *ctx, uint32_t hz);

    /* Assert (true) / release (false) the chip-select line. */
    void        (*cs)(void *ctx, bool assert_cs);
};

static inline hk_status_t hk_spi_xfer(const hk_spi_bus_t *b, const uint8_t *tx,
                                      uint8_t *rx, size_t len)
{
    return b->xfer(b->ctx, tx, rx, len);
}

static inline hk_status_t hk_spi_set_speed(const hk_spi_bus_t *b, uint32_t hz)
{
    return b->set_speed(b->ctx, hz);
}

static inline void hk_spi_cs(const hk_spi_bus_t *b, bool assert_cs)
{
    b->cs(b->ctx, assert_cs);
}

#ifdef __cplusplus
}
#endif

#endif /* HK_SPI_IF_H */
