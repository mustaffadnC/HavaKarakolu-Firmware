#include "mocks/mock_spi.h"

#include <string.h>

#include "common/crc.h"

/* ---- output queue ---- */

static void outq_push(hk_sd_model_t *m, uint8_t b)
{
    size_t next = (m->out_w + 1u) % HK_SDM_OUTQ;
    if (next != m->out_r) {
        m->outq[m->out_w] = b;
        m->out_w = next;
    }
}

static uint8_t outq_pop(hk_sd_model_t *m)
{
    if (m->out_r == m->out_w) {
        return 0xFF;
    }
    uint8_t b = m->outq[m->out_r];
    m->out_r = (m->out_r + 1u) % HK_SDM_OUTQ;
    return b;
}

/* ---- command handling ---- */

static uint32_t cmd_arg(const uint8_t *c)
{
    return ((uint32_t)c[1] << 24) | ((uint32_t)c[2] << 16) |
           ((uint32_t)c[3] << 8) | (uint32_t)c[4];
}

static uint32_t to_lba(const hk_sd_model_t *m, uint32_t addr)
{
    return m->sdhc ? addr : addr / HK_SDM_BLOCK;
}

static void respond_r1(hk_sd_model_t *m, uint8_t r1)
{
    outq_push(m, 0xFF);   /* Ncr gap */
    outq_push(m, r1);
}

static void handle_command(hk_sd_model_t *m)
{
    uint8_t  cmd = (uint8_t)(m->cmd[0] & 0x3Fu);
    uint32_t arg = cmd_arg(m->cmd);

    if (hk_crc7_sd(m->cmd, 5) != m->cmd[5]) {
        m->crc_errors++;
    }

    bool was_cmd55 = m->last_was_cmd55;
    m->last_was_cmd55 = false;

    switch (cmd) {
    case 0:   /* GO_IDLE */
        m->ready = false;
        respond_r1(m, 0x01);
        break;

    case 8:   /* SEND_IF_COND */
        if (m->v2) {
            respond_r1(m, 0x01);
            outq_push(m, 0x00);
            outq_push(m, 0x00);
            outq_push(m, (uint8_t)((arg >> 8) & 0x0Fu));
            outq_push(m, (uint8_t)(arg & 0xFFu));
        } else {
            respond_r1(m, 0x05);   /* idle + illegal command */
        }
        break;

    case 55:  /* APP_CMD */
        m->last_was_cmd55 = true;
        respond_r1(m, m->ready ? 0x00 : 0x01);
        break;

    case 41:  /* ACMD41 (only valid after CMD55) */
        if (!was_cmd55) {
            respond_r1(m, 0x05);
            break;
        }
        if (m->acmd41_polls > 0) {
            m->acmd41_polls--;
            respond_r1(m, 0x01);
        } else {
            m->ready = true;
            respond_r1(m, 0x00);
        }
        break;

    case 58:  /* READ_OCR */
        respond_r1(m, m->ready ? 0x00 : 0x01);
        outq_push(m, m->sdhc ? 0xC0 : 0x80);
        outq_push(m, 0xFF);
        outq_push(m, 0x80);
        outq_push(m, 0x00);
        break;

    case 16:  /* SET_BLOCKLEN */
        respond_r1(m, (arg == HK_SDM_BLOCK) ? 0x00 : 0x40);
        break;

    case 17: { /* READ_SINGLE_BLOCK */
        uint32_t lba = to_lba(m, arg);
        if (m->blocks == NULL || lba >= m->n_blocks) {
            respond_r1(m, 0x40);   /* parameter error */
            break;
        }
        respond_r1(m, 0x00);
        outq_push(m, 0xFF);        /* access-time gap */
        outq_push(m, 0xFE);        /* start token */
        const uint8_t *blk = &m->blocks[(size_t)lba * HK_SDM_BLOCK];
        for (size_t i = 0; i < HK_SDM_BLOCK; ++i) {
            outq_push(m, blk[i]);
        }
        uint16_t crc = hk_crc16_xmodem(blk, HK_SDM_BLOCK);
        if (m->corrupt_read_crc) {
            crc ^= 0x5A5Au;
        }
        outq_push(m, (uint8_t)(crc >> 8));
        outq_push(m, (uint8_t)(crc & 0xFFu));
        break;
    }

    case 24: { /* WRITE_SINGLE_BLOCK */
        uint32_t lba = to_lba(m, arg);
        if (m->blocks == NULL || lba >= m->n_blocks) {
            respond_r1(m, 0x40);
            break;
        }
        respond_r1(m, 0x00);
        m->rx_data_mode  = true;
        m->rx_token_seen = false;
        m->rx_pos        = 0;
        m->rx_lba        = lba;
        break;
    }

    default:
        respond_r1(m, 0x05);   /* illegal */
        break;
    }
}

static void finish_write(hk_sd_model_t *m)
{
    uint16_t crc_rx   = (uint16_t)(((uint16_t)m->rx_buf[HK_SDM_BLOCK] << 8) |
                                   m->rx_buf[HK_SDM_BLOCK + 1]);
    uint16_t crc_calc = hk_crc16_xmodem(m->rx_buf, HK_SDM_BLOCK);

    uint8_t resp = (m->write_response != 0) ? m->write_response
                   : ((crc_rx == crc_calc) ? 0x05 : 0x0B);
    if (resp == 0x05 && crc_rx == crc_calc) {
        memcpy(&m->blocks[(size_t)m->rx_lba * HK_SDM_BLOCK],
               m->rx_buf, HK_SDM_BLOCK);
    }
    outq_push(m, resp);
    outq_push(m, 0x00);   /* busy */
    outq_push(m, 0x00);
    outq_push(m, 0xFF);   /* released */
    m->rx_data_mode = false;
}

/* Process one incoming MOSI byte (after the MISO byte was emitted). */
static void process_in(hk_sd_model_t *m, uint8_t b)
{
    if (m->rx_data_mode) {
        if (!m->rx_token_seen) {
            if (b == 0xFE) {
                m->rx_token_seen = true;
            }
            return;
        }
        m->rx_buf[m->rx_pos++] = b;
        if (m->rx_pos == (int)sizeof(m->rx_buf)) {
            finish_write(m);
        }
        return;
    }

    if (m->cmd_len == 0) {
        /* command start byte: 01xxxxxx */
        if ((b & 0xC0u) == 0x40u) {
            m->cmd[m->cmd_len++] = b;
        }
        return;
    }
    m->cmd[m->cmd_len++] = b;
    if (m->cmd_len == 6) {
        m->cmd_len = 0;
        handle_command(m);
    }
}

/* ---- spi_if implementation ---- */

static hk_status_t model_xfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                              size_t len)
{
    hk_sd_model_t *m = (hk_sd_model_t *)ctx;
    for (size_t i = 0; i < len; ++i) {
        uint8_t out = m->absent ? 0xFF : outq_pop(m);
        if (rx != NULL) {
            rx[i] = out;
        }
        if (!m->absent) {
            process_in(m, (tx != NULL) ? tx[i] : 0xFF);
        }
    }
    return HK_OK;
}

static hk_status_t model_set_speed(void *ctx, uint32_t hz)
{
    hk_sd_model_t *m = (hk_sd_model_t *)ctx;
    if (m->speed_changes == 0) {
        m->first_speed_hz = hz;
    }
    m->last_speed_hz = hz;
    m->speed_changes++;
    return HK_OK;
}

static void model_cs(void *ctx, bool assert_cs)
{
    hk_sd_model_t *m = (hk_sd_model_t *)ctx;
    m->cs = assert_cs;
    if (!assert_cs) {
        m->cmd_len = 0;   /* frame sync is lost when deselected */
    }
}

void hk_sd_model_init(hk_sd_model_t *m, hk_spi_bus_t *bus,
                      uint8_t *blocks, size_t n_blocks)
{
    memset(m, 0, sizeof(*m));
    m->v2       = true;
    m->sdhc     = true;
    m->blocks   = blocks;
    m->n_blocks = n_blocks;

    bus->ctx       = m;
    bus->xfer      = model_xfer;
    bus->set_speed = model_set_speed;
    bus->cs        = model_cs;
}
