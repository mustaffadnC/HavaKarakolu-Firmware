#include "drivers/sd_spi/sd_spi.h"
#include "hk_test.h"
#include "mocks/mock_spi.h"

#include <string.h>

/* ---- hk_time shims ---- */
void     hk_time_init(void) {}
uint32_t hk_millis(void) { return 0; }
void     hk_delay_ms(uint32_t ms) { (void)ms; }
void     hk_delay_us(uint32_t us) { (void)us; }

#define N_BLOCKS 16u
static uint8_t s_blocks[N_BLOCKS * HK_SDM_BLOCK];

static void setup(hk_sd_model_t *m, hk_spi_bus_t *bus)
{
    memset(s_blocks, 0, sizeof(s_blocks));
    hk_sd_model_init(m, bus, s_blocks, N_BLOCKS);
}

static void test_init_sdhc_v2(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);
    HK_CHECK(sd.ready);
    HK_CHECK(sd.v2);
    HK_CHECK(sd.sdhc);
    HK_CHECK_EQ_INT(m.crc_errors, 0);
    /* init handshake at <= 400 kHz, then the fast data clock */
    HK_CHECK(m.first_speed_hz <= 400000u);
    HK_CHECK(m.last_speed_hz >= 1000000u);
}

static void test_init_sdsc(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);
    m.sdhc = false;

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);
    HK_CHECK(!sd.sdhc);
    HK_CHECK_EQ_INT(m.crc_errors, 0);
}

static void test_init_v1_card(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);
    m.v2   = false;
    m.sdhc = false;

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);
    HK_CHECK(!sd.v2);
    HK_CHECK(!sd.sdhc);
}

static void test_absent_card(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);
    m.absent = true;

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_ERR_NOT_FOUND);
    HK_CHECK(!sd.ready);
}

static void test_slow_acmd41(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);
    m.acmd41_polls = 50;

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);
    HK_CHECK(sd.ready);
}

static void test_rw_roundtrip_sdhc(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);

    uint8_t wr[HK_SD_BLOCK_SIZE], rd[HK_SD_BLOCK_SIZE];
    for (size_t i = 0; i < sizeof(wr); ++i) {
        wr[i] = (uint8_t)(i * 7u + 3u);
    }
    HK_CHECK_EQ_INT(hk_sd_write_block(&sd, 5, wr), HK_OK);
    /* landed in the model's backing store at LBA 5 */
    HK_CHECK(memcmp(&s_blocks[5u * HK_SDM_BLOCK], wr, sizeof(wr)) == 0);

    memset(rd, 0, sizeof(rd));
    HK_CHECK_EQ_INT(hk_sd_read_block(&sd, 5, rd), HK_OK);
    HK_CHECK(memcmp(rd, wr, sizeof(rd)) == 0);
    HK_CHECK_EQ_INT(m.crc_errors, 0);
}

static void test_rw_roundtrip_sdsc_addressing(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);
    m.sdhc = false;   /* byte addressing: driver must send lba*512 */

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);

    uint8_t wr[HK_SD_BLOCK_SIZE], rd[HK_SD_BLOCK_SIZE];
    memset(wr, 0xA5, sizeof(wr));
    HK_CHECK_EQ_INT(hk_sd_write_block(&sd, 3, wr), HK_OK);
    HK_CHECK(memcmp(&s_blocks[3u * HK_SDM_BLOCK], wr, sizeof(wr)) == 0);
    HK_CHECK_EQ_INT(hk_sd_read_block(&sd, 3, rd), HK_OK);
    HK_CHECK(memcmp(rd, wr, sizeof(rd)) == 0);
}

static void test_read_crc_error(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);

    m.corrupt_read_crc = true;
    uint8_t rd[HK_SD_BLOCK_SIZE];
    HK_CHECK_EQ_INT(hk_sd_read_block(&sd, 0, rd), HK_ERR_CRC);
}

static void test_write_rejected(void)
{
    hk_sd_model_t m;
    hk_spi_bus_t  bus;
    setup(&m, &bus);

    hk_sd_t sd;
    HK_CHECK_EQ_INT(hk_sd_init(&sd, &bus, 0), HK_OK);

    m.write_response = 0x0D;   /* write error */
    uint8_t wr[HK_SD_BLOCK_SIZE];
    memset(wr, 0x11, sizeof(wr));
    HK_CHECK_EQ_INT(hk_sd_write_block(&sd, 1, wr), HK_ERR_IO);
}

static void test_state_guards(void)
{
    hk_sd_t sd;
    memset(&sd, 0, sizeof(sd));
    uint8_t buf[HK_SD_BLOCK_SIZE];
    HK_CHECK_EQ_INT(hk_sd_read_block(&sd, 0, buf), HK_ERR_STATE);
    HK_CHECK_EQ_INT(hk_sd_write_block(&sd, 0, buf), HK_ERR_STATE);
    HK_CHECK_EQ_INT(hk_sd_init(NULL, NULL, 0), HK_ERR_PARAM);
}

int main(void)
{
    printf("test_sd_spi\n");
    test_init_sdhc_v2();
    test_init_sdsc();
    test_init_v1_card();
    test_absent_card();
    test_slow_acmd41();
    test_rw_roundtrip_sdhc();
    test_rw_roundtrip_sdsc_addressing();
    test_read_crc_error();
    test_write_rejected();
    test_state_guards();
    return hk_test_summary();
}
