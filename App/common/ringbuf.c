#include "ringbuf.h"

static bool is_pow2(size_t v)
{
    return v >= 2U && (v & (v - 1U)) == 0U;
}

bool hk_ringbuf_init(hk_ringbuf_t *rb, uint8_t *storage, size_t capacity)
{
    if (rb == NULL || storage == NULL || !is_pow2(capacity)) {
        return false;
    }
    rb->buf  = storage;
    rb->mask = capacity - 1U;
    rb->head = 0U;
    rb->tail = 0U;
    return true;
}

void hk_ringbuf_reset(hk_ringbuf_t *rb)
{
    rb->head = 0U;
    rb->tail = 0U;
}

size_t hk_ringbuf_count(const hk_ringbuf_t *rb)
{
    /* Single snapshot of each volatile index keeps this SPSC-safe. */
    size_t head = rb->head;
    size_t tail = rb->tail;
    return (head - tail) & rb->mask;
}

size_t hk_ringbuf_free(const hk_ringbuf_t *rb)
{
    return rb->mask - hk_ringbuf_count(rb); /* one slot reserved */
}

bool hk_ringbuf_is_empty(const hk_ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool hk_ringbuf_is_full(const hk_ringbuf_t *rb)
{
    return ((rb->head + 1U) & rb->mask) == rb->tail;
}

bool hk_ringbuf_put(hk_ringbuf_t *rb, uint8_t byte)
{
    size_t head = rb->head;
    size_t next = (head + 1U) & rb->mask;
    if (next == rb->tail) {
        return false; /* full */
    }
    rb->buf[head] = byte;
    rb->head = next;
    return true;
}

bool hk_ringbuf_get(hk_ringbuf_t *rb, uint8_t *out)
{
    size_t tail = rb->tail;
    if (tail == rb->head) {
        return false; /* empty */
    }
    *out = rb->buf[tail];
    rb->tail = (tail + 1U) & rb->mask;
    return true;
}

size_t hk_ringbuf_write(hk_ringbuf_t *rb, const uint8_t *data, size_t len)
{
    size_t n = 0;
    while (n < len && hk_ringbuf_put(rb, data[n])) {
        ++n;
    }
    return n;
}

size_t hk_ringbuf_read(hk_ringbuf_t *rb, uint8_t *out, size_t len)
{
    size_t n = 0;
    while (n < len && hk_ringbuf_get(rb, &out[n])) {
        ++n;
    }
    return n;
}
