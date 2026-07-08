#ifndef HK_RINGBUF_H
#define HK_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lock-free single-producer / single-consumer byte ring buffer.
 *
 * Safe without a mutex when exactly one context (e.g. an ISR) writes and
 * exactly one context (e.g. a task) reads. Capacity MUST be a power of two;
 * one slot is reserved so usable capacity is (capacity - 1) bytes.
 *
 * The backing storage is supplied by the caller (no dynamic allocation).
 */
typedef struct {
    uint8_t       *buf;
    size_t         mask;          /* capacity - 1, capacity is power of two */
    volatile size_t head;         /* write index (producer) */
    volatile size_t tail;         /* read index (consumer)  */
} hk_ringbuf_t;

/* Returns false if capacity is not a power of two or < 2. */
bool   hk_ringbuf_init(hk_ringbuf_t *rb, uint8_t *storage, size_t capacity);
void   hk_ringbuf_reset(hk_ringbuf_t *rb);

size_t hk_ringbuf_count(const hk_ringbuf_t *rb);
size_t hk_ringbuf_free(const hk_ringbuf_t *rb);
bool   hk_ringbuf_is_empty(const hk_ringbuf_t *rb);
bool   hk_ringbuf_is_full(const hk_ringbuf_t *rb);

/* Push one byte. Returns false if full. */
bool   hk_ringbuf_put(hk_ringbuf_t *rb, uint8_t byte);
/* Pop one byte into *out. Returns false if empty. */
bool   hk_ringbuf_get(hk_ringbuf_t *rb, uint8_t *out);

/* Bulk write; returns number of bytes actually written (<= len). */
size_t hk_ringbuf_write(hk_ringbuf_t *rb, const uint8_t *data, size_t len);
/* Bulk read; returns number of bytes actually read (<= len). */
size_t hk_ringbuf_read(hk_ringbuf_t *rb, uint8_t *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HK_RINGBUF_H */
