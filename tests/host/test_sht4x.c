#include "drivers/sht4x/sht4x.h"
#include "hk_test.h"

#include <math.h>

int main(void)
{
    printf("test_sht4x\n");

    /* T bytes = RH bytes = 0xBE 0xEF, CRC-8 = 0x92 (known good). */
    uint8_t raw[6] = { 0xBE, 0xEF, 0x92, 0xBE, 0xEF, 0x92 };
    float t = 0.0f, rh = 0.0f;

    HK_CHECK_EQ_INT(hk_sht4x_parse(raw, &t, &rh), HK_OK);

    /* ticks = 0xBEEF = 48879 -> T = -45 + 175*48879/65535 ≈ 85.52 °C */
    HK_CHECK(fabsf(t - 85.52f) < 0.5f);
    /* RH = -6 + 125*48879/65535 ≈ 87.23 % */
    HK_CHECK(fabsf(rh - 87.23f) < 0.5f);

    /* Corrupt temperature CRC -> must reject. */
    raw[2] = 0x00;
    HK_CHECK_EQ_INT(hk_sht4x_parse(raw, &t, &rh), HK_ERR_CRC);

    /* NULL guard. */
    HK_CHECK_EQ_INT(hk_sht4x_parse(NULL, &t, &rh), HK_ERR_PARAM);

    return hk_test_summary();
}
