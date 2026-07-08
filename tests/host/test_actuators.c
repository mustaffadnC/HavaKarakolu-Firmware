#include "drivers/battery/battery.h"
#include "drivers/buzzer/buzzer.h"
#include "drivers/fan/fan.h"
#include "drivers/servo/servo.h"
#include "drivers/ws2812/ws2812.h"
#include "hk_test.h"

#include <math.h>

int main(void)
{
    printf("test_actuators\n");

    /* ---- servo angle -> pulse ---- */
    HK_CHECK_EQ_INT(hk_servo_angle_to_us(0.0f,   1000, 2000), 1000);
    HK_CHECK_EQ_INT(hk_servo_angle_to_us(180.0f, 1000, 2000), 2000);
    HK_CHECK_EQ_INT(hk_servo_angle_to_us(90.0f,  1000, 2000), 1500);
    HK_CHECK_EQ_INT(hk_servo_angle_to_us(-50.0f, 1000, 2000), 1000); /* clamp */

    /* ---- buzzer timer calc (84 MHz timer clock) ---- */
    uint16_t psc = 0, arr = 0;
    HK_CHECK_EQ_INT(hk_buzzer_calc(84000000u, 1000u, &psc, &arr), HK_OK);
    HK_CHECK_EQ_INT(psc, 1);
    HK_CHECK_EQ_INT(arr, 41999);
    /* resulting frequency == 84e6 / ((psc+1)*(arr+1)) == 1000 */
    HK_CHECK_EQ_INT(84000000u / ((uint32_t)(psc + 1) * (arr + 1)), 1000);
    /* frequency too high -> param error */
    HK_CHECK_EQ_INT(hk_buzzer_calc(84000000u, 84000000u, &psc, &arr), HK_ERR_PARAM);

    /* ---- fan hysteresis thermostat (on>=35, off<=30) ---- */
    HK_CHECK(!hk_fan_thermostat(30.0f, 35.0f, 30.0f, false)); /* off stays off */
    HK_CHECK( hk_fan_thermostat(36.0f, 35.0f, 30.0f, false)); /* off -> on */
    HK_CHECK( hk_fan_thermostat(31.0f, 35.0f, 30.0f, true));  /* on stays on */
    HK_CHECK(!hk_fan_thermostat(30.0f, 35.0f, 30.0f, true));  /* on -> off */

    /* ---- battery voltage + SoC ---- */
    float v = hk_battery_voltage(2048, 3.3f, 4095.0f, 5.54545f);
    HK_CHECK(fabsf(v - 9.153f) < 0.05f);
    HK_CHECK(fabsf(hk_battery_soc(11.1f, 3) - 0.444f) < 0.02f);
    HK_CHECK(fabsf(hk_battery_soc(12.6f, 3) - 1.0f)   < 0.001f);
    HK_CHECK(fabsf(hk_battery_soc(9.9f,  3) - 0.0f)   < 0.001f);

    /* ---- WS2812 GRB encode ---- */
    uint16_t buf[HK_WS2812_BITS];
    hk_ws2812_encode(0x01, 0x80, 0x00, 58, 29, buf);
    HK_CHECK_EQ_INT(buf[0],  58);  /* G bit7 = 1 */
    HK_CHECK_EQ_INT(buf[1],  29);  /* G bit6 = 0 */
    HK_CHECK_EQ_INT(buf[8],  29);  /* R bit7 = 0 */
    HK_CHECK_EQ_INT(buf[15], 58);  /* R bit0 = 1 */
    HK_CHECK_EQ_INT(buf[16], 29);  /* B bit7 = 0 */
    HK_CHECK_EQ_INT(buf[23], 29);  /* B bit0 = 0 */

    return hk_test_summary();
}
