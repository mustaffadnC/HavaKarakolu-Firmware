#include "common/filters.h"
#include "services/health/health.h"
#include "services/mission/mission.h"
#include "hk_test.h"

#include <math.h>
#include <string.h>

int main(void)
{
    printf("test_services\n");

    /* ---- complementary filter ---- */
    hk_comp_filter_t f;
    hk_vec3f g_zero = { 0.0f, 0.0f, 0.0f };

    /* pure accel (alpha=0), level -> ~0 roll/pitch */
    hk_comp_filter_init(&f, 0.0f);
    hk_vec3f a_level = { 0.0f, 0.0f, 9.81f };
    hk_comp_filter_update(&f, &a_level, &g_zero, 0.01f);
    HK_CHECK(fabsf(f.roll_deg)  < 0.5f);
    HK_CHECK(fabsf(f.pitch_deg) < 0.5f);

    /* pure accel, rolled 90° (gravity on +Y) */
    hk_comp_filter_init(&f, 0.0f);
    hk_vec3f a_roll = { 0.0f, 9.81f, 0.0f };
    hk_comp_filter_update(&f, &a_roll, &g_zero, 0.01f);
    HK_CHECK(fabsf(f.roll_deg - 90.0f) < 1.0f);

    /* pure gyro (alpha=1): 1 rad/s for 1 s -> 57.30° */
    hk_comp_filter_init(&f, 1.0f);
    hk_vec3f g_roll = { 1.0f, 0.0f, 0.0f };
    hk_comp_filter_update(&f, &a_level, &g_roll, 1.0f);
    HK_CHECK(fabsf(f.roll_deg - 57.2958f) < 0.5f);

    /* ---- health task-alive ---- */
    HK_CHECK(hk_health_all_alive(0x0F, 0x0F));
    HK_CHECK(!hk_health_all_alive(0x0E, 0x0F));
    HK_CHECK(hk_health_all_alive(0xFF, 0x0F));   /* extra alive bits are fine */

    /* ---- mission state names ---- */
    HK_CHECK(strcmp(hk_mission_state_str(HK_MISSION_ARMED), "ARMED") == 0);
    HK_CHECK(strcmp(hk_mission_state_str(HK_MISSION_RECOVERY), "RECOVERY") == 0);

    return hk_test_summary();
}
