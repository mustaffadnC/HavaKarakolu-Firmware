#include "common/ringbuf.h"
#include "hk_test.h"

int main(void)
{
    printf("test_ringbuf\n");

    uint8_t      storage[8];
    hk_ringbuf_t rb;

    /* Non power-of-two must be rejected. */
    HK_CHECK(!hk_ringbuf_init(&rb, storage, 7));
    /* Valid init: capacity 8 -> usable 7. */
    HK_CHECK(hk_ringbuf_init(&rb, storage, 8));

    HK_CHECK(hk_ringbuf_is_empty(&rb));
    HK_CHECK_EQ_INT(hk_ringbuf_count(&rb), 0);
    HK_CHECK_EQ_INT(hk_ringbuf_free(&rb), 7);

    /* Fill to capacity (7 bytes), then it must report full. */
    for (int i = 0; i < 7; ++i) {
        HK_CHECK(hk_ringbuf_put(&rb, (uint8_t)(i + 1)));
    }
    HK_CHECK(hk_ringbuf_is_full(&rb));
    HK_CHECK(!hk_ringbuf_put(&rb, 0xFF));       /* full -> reject */
    HK_CHECK_EQ_INT(hk_ringbuf_count(&rb), 7);

    /* Drain in FIFO order. */
    for (int i = 0; i < 7; ++i) {
        uint8_t v = 0;
        HK_CHECK(hk_ringbuf_get(&rb, &v));
        HK_CHECK_EQ_INT(v, i + 1);
    }
    HK_CHECK(hk_ringbuf_is_empty(&rb));
    uint8_t dummy;
    HK_CHECK(!hk_ringbuf_get(&rb, &dummy));     /* empty -> reject */

    /* Wrap-around: head/tail advanced; bulk write/read across boundary. */
    const uint8_t in[5] = { 10, 20, 30, 40, 50 };
    HK_CHECK_EQ_INT(hk_ringbuf_write(&rb, in, 5), 5);
    uint8_t out[5] = { 0 };
    HK_CHECK_EQ_INT(hk_ringbuf_read(&rb, out, 5), 5);
    for (int i = 0; i < 5; ++i) {
        HK_CHECK_EQ_INT(out[i], in[i]);
    }

    /* Bulk write honours free space (only 7 fit). */
    const uint8_t big[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    HK_CHECK_EQ_INT(hk_ringbuf_write(&rb, big, 10), 7);
    HK_CHECK(hk_ringbuf_is_full(&rb));

    return hk_test_summary();
}
