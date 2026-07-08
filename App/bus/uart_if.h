#ifndef HK_UART_IF_H
#define HK_UART_IF_H

#include <stddef.h>
#include <stdint.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Abstract byte-stream UART. Concrete implementations:
 *   - uart_dma.c : STM32 HAL UART, DMA RX into a ring buffer + IDLE line,
 *                  interrupt/DMA TX queue.
 *   - mock       : host unit tests (and the telemetry transport shim).
 *
 * read()/available() are non-blocking and operate on the RX ring buffer.
 * write() enqueues bytes for transmission.
 */
typedef struct hk_uart hk_uart_t;

struct hk_uart {
    void *ctx;

    /* Enqueue `len` bytes for TX. Returns bytes accepted, or <0 (hk_status_t). */
    int    (*write)(void *ctx, const uint8_t *data, size_t len);

    /* Copy up to `len` received bytes into `out`. Returns count copied. */
    size_t (*read)(void *ctx, uint8_t *out, size_t len);

    /* Number of received bytes currently available. */
    size_t (*available)(void *ctx);
};

static inline int hk_uart_write(const hk_uart_t *u, const uint8_t *data, size_t len)
{
    return u->write(u->ctx, data, len);
}

static inline size_t hk_uart_read(const hk_uart_t *u, uint8_t *out, size_t len)
{
    return u->read(u->ctx, out, len);
}

static inline size_t hk_uart_available(const hk_uart_t *u)
{
    return u->available(u->ctx);
}

#ifdef __cplusplus
}
#endif

#endif /* HK_UART_IF_H */
