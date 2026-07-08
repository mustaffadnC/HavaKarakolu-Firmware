#include "common/crc.h"
#include "hk_test.h"

#include <string.h>

int main(void)
{
    printf("test_crc\n");

    /* Sensirion CRC-8 datasheet example: 0xBE 0xEF -> 0x92 */
    const uint8_t beef[] = { 0xBE, 0xEF };
    HK_CHECK_EQ_INT(hk_crc8_sensirion(beef, sizeof(beef)), 0x92);

    /* CRC-16/CCITT-FALSE check value for ASCII "123456789" is 0x29B1 */
    const uint8_t check[] = "123456789";
    HK_CHECK_EQ_INT(hk_crc16_ccitt(check, 9), 0x29B1);

    /* Streaming must match one-shot. */
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < 9; ++i) {
        crc = hk_crc16_ccitt_update(crc, check[i]);
    }
    HK_CHECK_EQ_INT(crc, 0x29B1);

    /* Empty input returns the seed. */
    HK_CHECK_EQ_INT(hk_crc16_ccitt(NULL, 0), 0xFFFF);

    return hk_test_summary();
}
