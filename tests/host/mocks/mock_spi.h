#ifndef HK_MOCK_SPI_H
#define HK_MOCK_SPI_H

/*
 * Behavioral SD-card model behind an hk_spi_bus_t for host tests.
 *
 * Emulates the SPI-mode protocol byte-by-byte: CMD0/CMD8/CMD55+ACMD41/
 * CMD58/CMD16 init handshake, CMD17 single-block read (token + data + CRC16),
 * CMD24 single-block write (token detect, CRC verify, data response, busy).
 *
 * Knobs let tests exercise: v1 vs v2 cards, SDSC vs SDHC addressing, missing
 * card, slow ACMD41, corrupted read CRC and write rejection.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus/spi_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HK_SDM_BLOCK      512u
#define HK_SDM_OUTQ       2048u

typedef struct {
    /* configuration (set before init) */
    bool     absent;            /* no card: MISO always 0xFF            */
    bool     v2;                /* CMD8 supported                       */
    bool     sdhc;              /* OCR CCS bit + LBA addressing         */
    int      acmd41_polls;      /* busy responses before ready          */
    bool     corrupt_read_crc;  /* flip a CRC byte on CMD17 replies     */
    uint8_t  write_response;    /* data response token; 0 -> 0x05       */

    /* card backing storage */
    uint8_t *blocks;
    size_t   n_blocks;

    /* observability for asserts */
    int      crc_errors;        /* command frames with bad CRC7         */
    uint32_t first_speed_hz;
    uint32_t last_speed_hz;
    int      speed_changes;
    bool     cs;

    /* internals */
    bool     ready;
    bool     last_was_cmd55;
    uint8_t  cmd[6];
    int      cmd_len;
    bool     rx_data_mode;      /* collecting a CMD24 data packet       */
    uint32_t rx_lba;
    int      rx_pos;
    bool     rx_token_seen;
    uint8_t  rx_buf[HK_SDM_BLOCK + 2];
    uint8_t  outq[HK_SDM_OUTQ];
    size_t   out_r, out_w;
} hk_sd_model_t;

/* Wire `bus` to the model. `blocks` may be NULL if no data ops are tested. */
void hk_sd_model_init(hk_sd_model_t *m, hk_spi_bus_t *bus,
                      uint8_t *blocks, size_t n_blocks);

#ifdef __cplusplus
}
#endif

#endif /* HK_MOCK_SPI_H */
