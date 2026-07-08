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

    /* CRC-16/XMODEM check value for "123456789" is 0x31C3 (SD data blocks). */
    HK_CHECK_EQ_INT(hk_crc16_xmodem(check, 9), 0x31C3);

    /* CRC-7 for SD command frames (result includes the end bit):
     * CMD0  {0x40,0,0,0,0}          -> 0x95
     * CMD8  {0x48,0,0,0x01,0xAA}    -> 0x87 */
    const uint8_t cmd0[] = { 0x40, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t cmd8[] = { 0x48, 0x00, 0x00, 0x01, 0xAA };
    HK_CHECK_EQ_INT(hk_crc7_sd(cmd0, sizeof(cmd0)), 0x95);
    HK_CHECK_EQ_INT(hk_crc7_sd(cmd8, sizeof(cmd8)), 0x87);

    /* CRC-32/ISO-HDLC check value for "123456789" is 0xCBF43926. */
    HK_CHECK((long)hk_crc32(check, 9) == (long)0xCBF43926u);

    return hk_test_summary();
}
