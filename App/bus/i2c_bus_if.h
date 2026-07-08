#ifndef HK_I2C_BUS_IF_H
#define HK_I2C_BUS_IF_H

#include <stddef.h>
#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Abstract I2C bus. Concrete implementations:
 *   - i2c_hw.c : STM32 HAL I2C1/I2C2 (DMA + RTOS mutex/semaphore)
 *   - i2c_sw.c : bit-banged GPIO (PB8/PB9 for the second SHT4x)
 *   - mock     : host unit tests
 *
 * All addresses are 7-bit (the implementation shifts as needed).
 * All functions return hk_status_t (HK_OK == 0).
 */
typedef struct hk_i2c_bus hk_i2c_bus_t;

struct hk_i2c_bus {
    void *ctx;

    /* Raw write of `len` bytes (e.g. SHT4x command). */
    hk_status_t (*write)(void *ctx, uint8_t addr7,
                         const uint8_t *data, size_t len);

    /* Raw read of `len` bytes (e.g. SHT4x measurement result). */
    hk_status_t (*read)(void *ctx, uint8_t addr7,
                        uint8_t *buf, size_t len);

    /* Write `wlen` bytes then repeated-START read `rlen` bytes
       (register-based devices: BMI270, BMP581). */
    hk_status_t (*write_read)(void *ctx, uint8_t addr7,
                              const uint8_t *wbuf, size_t wlen,
                              uint8_t *rbuf, size_t rlen);

    /* Returns HK_OK if a device ACKs at addr7 (bus scan / presence). */
    hk_status_t (*probe)(void *ctx, uint8_t addr7);

    /* Attempt to clear a stuck bus (SCL clock-out + re-init). May be NULL. */
    void        (*recover)(void *ctx);
};

/* Convenience inline wrappers (null-safe on optional ops). */

static inline hk_status_t hk_i2c_write(const hk_i2c_bus_t *b, uint8_t addr7,
                                       const uint8_t *data, size_t len)
{
    return b->write(b->ctx, addr7, data, len);
}

static inline hk_status_t hk_i2c_read(const hk_i2c_bus_t *b, uint8_t addr7,
                                      uint8_t *buf, size_t len)
{
    return b->read(b->ctx, addr7, buf, len);
}

static inline hk_status_t hk_i2c_write_read(const hk_i2c_bus_t *b, uint8_t addr7,
                                            const uint8_t *wbuf, size_t wlen,
                                            uint8_t *rbuf, size_t rlen)
{
    return b->write_read(b->ctx, addr7, wbuf, wlen, rbuf, rlen);
}

/* Read a single register (1-byte register address). */
static inline hk_status_t hk_i2c_read_reg(const hk_i2c_bus_t *b, uint8_t addr7,
                                          uint8_t reg, uint8_t *buf, size_t len)
{
    return b->write_read(b->ctx, addr7, &reg, 1, buf, len);
}

/* Write a single register byte. */
static inline hk_status_t hk_i2c_write_reg(const hk_i2c_bus_t *b, uint8_t addr7,
                                           uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg, value };
    return b->write(b->ctx, addr7, tx, sizeof(tx));
}

static inline hk_status_t hk_i2c_probe(const hk_i2c_bus_t *b, uint8_t addr7)
{
    return b->probe(b->ctx, addr7);
}

static inline void hk_i2c_recover(const hk_i2c_bus_t *b)
{
    if (b->recover != NULL) {
        b->recover(b->ctx);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HK_I2C_BUS_IF_H */
