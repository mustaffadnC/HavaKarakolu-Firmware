#include "mocks/mock_i2c.h"

#include <stdio.h>
#include <string.h>

static const hk_mock_i2c_step_t *next_step(hk_mock_i2c_t *m, const char *op)
{
    if (m->idx >= m->n) {
        printf("  mock_i2c: unexpected %s (script exhausted at %zu)\n",
               op, m->idx);
        m->mismatch++;
        return NULL;
    }
    return &m->steps[m->idx++];
}

static void check_write(hk_mock_i2c_t *m, const hk_mock_i2c_step_t *s,
                        uint8_t addr, const uint8_t *data, size_t len)
{
    if (addr != s->addr) {
        printf("  mock_i2c[%zu]: addr 0x%02X, want 0x%02X\n",
               m->idx - 1, addr, s->addr);
        m->mismatch++;
    }
    if (len != s->wlen) {
        printf("  mock_i2c[%zu]: wlen %zu, want %zu\n", m->idx - 1, len, s->wlen);
        m->mismatch++;
        return;
    }
    if (s->expect_w != NULL && len > 0 && memcmp(data, s->expect_w, len) != 0) {
        printf("  mock_i2c[%zu]: write bytes differ (first=0x%02X want=0x%02X)\n",
               m->idx - 1, data[0], s->expect_w[0]);
        m->mismatch++;
    }
}

static void do_reply(hk_mock_i2c_t *m, const hk_mock_i2c_step_t *s,
                     uint8_t *buf, size_t len)
{
    if (len != s->rlen) {
        printf("  mock_i2c[%zu]: rlen %zu, want %zu\n", m->idx - 1, len, s->rlen);
        m->mismatch++;
    }
    if (s->reply != NULL && buf != NULL) {
        size_t n = (len < s->rlen) ? len : s->rlen;
        memcpy(buf, s->reply, n);
    }
}

static hk_status_t mock_write(void *ctx, uint8_t addr7,
                              const uint8_t *data, size_t len)
{
    hk_mock_i2c_t *m = (hk_mock_i2c_t *)ctx;
    const hk_mock_i2c_step_t *s = next_step(m, "write");
    if (s == NULL) {
        return HK_ERR_IO;
    }
    check_write(m, s, addr7, data, len);
    return s->ret;
}

static hk_status_t mock_read(void *ctx, uint8_t addr7, uint8_t *buf, size_t len)
{
    hk_mock_i2c_t *m = (hk_mock_i2c_t *)ctx;
    const hk_mock_i2c_step_t *s = next_step(m, "read");
    if (s == NULL) {
        return HK_ERR_IO;
    }
    if (addr7 != s->addr) {
        m->mismatch++;
    }
    do_reply(m, s, buf, len);
    return s->ret;
}

static hk_status_t mock_write_read(void *ctx, uint8_t addr7,
                                   const uint8_t *wbuf, size_t wlen,
                                   uint8_t *rbuf, size_t rlen)
{
    hk_mock_i2c_t *m = (hk_mock_i2c_t *)ctx;
    const hk_mock_i2c_step_t *s = next_step(m, "write_read");
    if (s == NULL) {
        return HK_ERR_IO;
    }
    check_write(m, s, addr7, wbuf, wlen);
    do_reply(m, s, rbuf, rlen);
    return s->ret;
}

static hk_status_t mock_probe(void *ctx, uint8_t addr7)
{
    (void)ctx; (void)addr7;
    return HK_OK;
}

void hk_mock_i2c_init(hk_mock_i2c_t *m, hk_i2c_bus_t *bus,
                      const hk_mock_i2c_step_t *steps, size_t n)
{
    m->steps    = steps;
    m->n        = n;
    m->idx      = 0;
    m->mismatch = 0;

    bus->ctx        = m;
    bus->write      = mock_write;
    bus->read       = mock_read;
    bus->write_read = mock_write_read;
    bus->probe      = mock_probe;
    bus->recover    = NULL;
}

bool hk_mock_i2c_done(const hk_mock_i2c_t *m)
{
    if (m->idx != m->n) {
        printf("  mock_i2c: %zu of %zu steps consumed\n", m->idx, m->n);
        return false;
    }
    return m->mismatch == 0;
}
