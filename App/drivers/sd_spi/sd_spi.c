#include "drivers/sd_spi/sd_spi.h"

#include <stddef.h>

#include "common/crc.h"
#include "common/hk_time.h"

/* ---- commands ---- */
#define SD_CMD0_GO_IDLE        0
#define SD_CMD1_SEND_OP_COND   1
#define SD_CMD8_SEND_IF_COND   8
#define SD_CMD16_SET_BLOCKLEN  16
#define SD_CMD17_READ_SINGLE   17
#define SD_CMD24_WRITE_SINGLE  24
#define SD_CMD55_APP_CMD       55
#define SD_CMD58_READ_OCR      58
#define SD_ACMD41_SD_OP_COND   41

/* ---- R1 bits ---- */
#define SD_R1_IDLE             0x01u
#define SD_R1_ILLEGAL_CMD      0x04u
#define SD_R1_NO_RESPONSE      0xFFu

#define SD_TOKEN_START         0xFEu
#define SD_DATA_ACCEPTED       0x05u

#define SD_INIT_SPEED_HZ       400000u
#define SD_DEFAULT_DATA_HZ     10500000u

/* Poll budgets (each poll exchanges one byte; delays keep RTOS friendly). */
#define SD_R1_POLL_BYTES       8
#define SD_ACMD41_TIMEOUT_MS   1000u
#define SD_TOKEN_POLL          2000     /* ~read access time at slow clock */
#define SD_BUSY_POLL           25000    /* worst-case write busy ~250 ms   */

static hk_status_t xchg(const hk_spi_bus_t *spi, uint8_t out, uint8_t *in)
{
    return hk_spi_xfer(spi, &out, in, 1);
}

/* Send a command frame and return the R1 response.
 * CS must already be asserted; leaves CS asserted. */
static uint8_t send_cmd(hk_sd_t *sd, uint8_t cmd, uint32_t arg)
{
    uint8_t frame[6];
    frame[0] = (uint8_t)(0x40u | cmd);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg >> 0);
    frame[5] = hk_crc7_sd(frame, 5);

    /* one flush byte gives slow cards time to release the bus */
    uint8_t dummy;
    (void)xchg(sd->spi, 0xFF, &dummy);

    if (hk_spi_xfer(sd->spi, frame, NULL, sizeof(frame)) != HK_OK) {
        return SD_R1_NO_RESPONSE;
    }

    uint8_t r1 = SD_R1_NO_RESPONSE;
    for (int i = 0; i < SD_R1_POLL_BYTES; ++i) {
        if (xchg(sd->spi, 0xFF, &r1) != HK_OK) {
            return SD_R1_NO_RESPONSE;
        }
        if ((r1 & 0x80u) == 0u) {
            break;
        }
    }
    return r1;
}

/* Read the 4 trailing bytes of an R3/R7 response. */
static hk_status_t read_r3_r7_tail(hk_sd_t *sd, uint32_t *out)
{
    uint8_t b[4];
    hk_status_t st = hk_spi_xfer(sd->spi, NULL, b, sizeof(b));
    if (st != HK_OK) {
        return st;
    }
    *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
    return HK_OK;
}

/* Release CS and clock one byte so the card frees MISO. */
static void deselect(hk_sd_t *sd)
{
    hk_spi_cs(sd->spi, false);
    uint8_t dummy;
    (void)xchg(sd->spi, 0xFF, &dummy);
}

static hk_status_t wait_not_busy(hk_sd_t *sd, int polls)
{
    for (int i = 0; i < polls; ++i) {
        uint8_t b;
        hk_status_t st = xchg(sd->spi, 0xFF, &b);
        if (st != HK_OK) {
            return st;
        }
        if (b == 0xFFu) {
            return HK_OK;
        }
    }
    return HK_ERR_TIMEOUT;
}

hk_status_t hk_sd_wait_ready(hk_sd_t *sd)
{
    if (sd == NULL || !sd->ready) {
        return HK_ERR_STATE;
    }
    hk_spi_cs(sd->spi, true);
    hk_status_t st = wait_not_busy(sd, SD_BUSY_POLL);
    deselect(sd);
    return st;
}

hk_status_t hk_sd_init(hk_sd_t *sd, const hk_spi_bus_t *spi,
                       uint32_t data_speed_hz)
{
    if (sd == NULL || spi == NULL) {
        return HK_ERR_PARAM;
    }
    sd->spi           = spi;
    sd->data_speed_hz = (data_speed_hz == 0) ? SD_DEFAULT_DATA_HZ : data_speed_hz;
    sd->sdhc          = false;
    sd->v2            = false;
    sd->ready         = false;

    hk_status_t st = hk_spi_set_speed(spi, SD_INIT_SPEED_HZ);
    if (st != HK_OK) {
        return st;
    }

    /* >= 74 clocks with CS high puts the card into SPI-capable state. */
    hk_spi_cs(spi, false);
    uint8_t warmup[10];
    st = hk_spi_xfer(spi, NULL, warmup, sizeof(warmup));
    if (st != HK_OK) {
        return st;
    }

    /* CMD0: enter idle state. No response at all => no card present. */
    hk_spi_cs(spi, true);
    uint8_t r1 = SD_R1_NO_RESPONSE;
    for (int attempt = 0; attempt < 4 && r1 != SD_R1_IDLE; ++attempt) {
        r1 = send_cmd(sd, SD_CMD0_GO_IDLE, 0);
    }
    if (r1 == SD_R1_NO_RESPONSE) {
        deselect(sd);
        return HK_ERR_NOT_FOUND;
    }
    if (r1 != SD_R1_IDLE) {
        deselect(sd);
        return HK_ERR_IO;
    }

    /* CMD8: v2 detection. Pattern 0x1AA must echo back on v2 cards. */
    r1 = send_cmd(sd, SD_CMD8_SEND_IF_COND, 0x000001AAu);
    if ((r1 & SD_R1_ILLEGAL_CMD) == 0u) {
        uint32_t echo = 0;
        st = read_r3_r7_tail(sd, &echo);
        if (st != HK_OK || (echo & 0x00000FFFu) != 0x1AAu) {
            deselect(sd);
            return HK_ERR_IO;
        }
        sd->v2 = true;
    }

    /* ACMD41 (HCS for v2) until the card leaves idle. */
    uint32_t acmd_arg = sd->v2 ? 0x40000000u : 0u;
    uint32_t waited_ms = 0;
    for (;;) {
        r1 = send_cmd(sd, SD_CMD55_APP_CMD, 0);
        if (r1 == SD_R1_NO_RESPONSE) {
            deselect(sd);
            return HK_ERR_TIMEOUT;
        }
        r1 = send_cmd(sd, SD_ACMD41_SD_OP_COND, acmd_arg);
        if (r1 == 0x00u) {
            break;
        }
        if ((r1 & SD_R1_ILLEGAL_CMD) != 0u) {
            /* very old MMC: not supported on this design */
            deselect(sd);
            return HK_ERR_NOT_FOUND;
        }
        if (waited_ms >= SD_ACMD41_TIMEOUT_MS) {
            deselect(sd);
            return HK_ERR_TIMEOUT;
        }
        hk_delay_ms(1);
        waited_ms += 1;
    }

    /* CMD58: OCR -> CCS bit decides block vs byte addressing. */
    if (sd->v2) {
        r1 = send_cmd(sd, SD_CMD58_READ_OCR, 0);
        if (r1 != 0x00u) {
            deselect(sd);
            return HK_ERR_IO;
        }
        uint32_t ocr = 0;
        st = read_r3_r7_tail(sd, &ocr);
        if (st != HK_OK) {
            deselect(sd);
            return st;
        }
        sd->sdhc = (ocr & 0x40000000u) != 0u;
    }

    /* SDSC uses byte addressing: force 512-byte blocks. */
    if (!sd->sdhc) {
        r1 = send_cmd(sd, SD_CMD16_SET_BLOCKLEN, HK_SD_BLOCK_SIZE);
        if (r1 != 0x00u) {
            deselect(sd);
            return HK_ERR_IO;
        }
    }

    deselect(sd);

    st = hk_spi_set_speed(spi, sd->data_speed_hz);
    if (st != HK_OK) {
        return st;
    }

    sd->ready = true;
    return HK_OK;
}

static uint32_t block_addr(const hk_sd_t *sd, uint32_t lba)
{
    return sd->sdhc ? lba : lba * HK_SD_BLOCK_SIZE;
}

hk_status_t hk_sd_read_block(hk_sd_t *sd, uint32_t lba,
                             uint8_t buf[HK_SD_BLOCK_SIZE])
{
    if (sd == NULL || buf == NULL) {
        return HK_ERR_PARAM;
    }
    if (!sd->ready) {
        return HK_ERR_STATE;
    }

    hk_spi_cs(sd->spi, true);

    uint8_t r1 = send_cmd(sd, SD_CMD17_READ_SINGLE, block_addr(sd, lba));
    if (r1 != 0x00u) {
        deselect(sd);
        return (r1 == SD_R1_NO_RESPONSE) ? HK_ERR_TIMEOUT : HK_ERR_IO;
    }

    /* wait for the data start token */
    uint8_t tok = 0xFFu;
    int polls = 0;
    while (tok == 0xFFu) {
        if (xchg(sd->spi, 0xFF, &tok) != HK_OK) {
            deselect(sd);
            return HK_ERR_IO;
        }
        if (tok != 0xFFu) {
            break;
        }
        if (++polls >= SD_TOKEN_POLL) {
            deselect(sd);
            return HK_ERR_TIMEOUT;
        }
    }
    if (tok != SD_TOKEN_START) {
        deselect(sd);
        return HK_ERR_IO;   /* data error token */
    }

    hk_status_t st = hk_spi_xfer(sd->spi, NULL, buf, HK_SD_BLOCK_SIZE);
    uint8_t crc_raw[2] = {0, 0};
    if (st == HK_OK) {
        st = hk_spi_xfer(sd->spi, NULL, crc_raw, 2);
    }
    deselect(sd);
    if (st != HK_OK) {
        return st;
    }

    uint16_t crc_rx   = (uint16_t)(((uint16_t)crc_raw[0] << 8) | crc_raw[1]);
    uint16_t crc_calc = hk_crc16_xmodem(buf, HK_SD_BLOCK_SIZE);
    return (crc_rx == crc_calc) ? HK_OK : HK_ERR_CRC;
}

hk_status_t hk_sd_write_block(hk_sd_t *sd, uint32_t lba,
                              const uint8_t buf[HK_SD_BLOCK_SIZE])
{
    if (sd == NULL || buf == NULL) {
        return HK_ERR_PARAM;
    }
    if (!sd->ready) {
        return HK_ERR_STATE;
    }

    hk_spi_cs(sd->spi, true);

    /* the card may still be programming a previous block */
    if (wait_not_busy(sd, SD_BUSY_POLL) != HK_OK) {
        deselect(sd);
        return HK_ERR_TIMEOUT;
    }

    uint8_t r1 = send_cmd(sd, SD_CMD24_WRITE_SINGLE, block_addr(sd, lba));
    if (r1 != 0x00u) {
        deselect(sd);
        return (r1 == SD_R1_NO_RESPONSE) ? HK_ERR_TIMEOUT : HK_ERR_IO;
    }

    uint16_t crc = hk_crc16_xmodem(buf, HK_SD_BLOCK_SIZE);
    uint8_t  head[1] = { SD_TOKEN_START };
    uint8_t  tail[2] = { (uint8_t)(crc >> 8), (uint8_t)(crc & 0xFFu) };

    hk_status_t st = hk_spi_xfer(sd->spi, head, NULL, 1);
    if (st == HK_OK) { st = hk_spi_xfer(sd->spi, buf, NULL, HK_SD_BLOCK_SIZE); }
    if (st == HK_OK) { st = hk_spi_xfer(sd->spi, tail, NULL, 2); }
    if (st != HK_OK) {
        deselect(sd);
        return st;
    }

    /* data response: xxx0sss1 -> sss: 010 accepted, 101 CRC, 110 write error */
    uint8_t resp = 0xFFu;
    if (xchg(sd->spi, 0xFF, &resp) != HK_OK) {
        deselect(sd);
        return HK_ERR_IO;
    }
    if ((resp & 0x1Fu) != SD_DATA_ACCEPTED) {
        deselect(sd);
        return ((resp & 0x1Fu) == 0x0Bu) ? HK_ERR_CRC : HK_ERR_IO;
    }

    /* card holds MISO low while programming */
    st = wait_not_busy(sd, SD_BUSY_POLL);
    deselect(sd);
    return st;
}
