#include "tasks/app.h"

#if !defined(HK_HOST)

#include "FreeRTOS.h"
#include "task.h"

#include "bsp/board_config.h"
#include "common/hk_time.h"
#include "common/log.h"
#include "services/health/health.h"
#include "services/mission/mission.h"
#include "services/system_state/system_state.h"
#include "tasks/app_config.h"

extern IWDG_HandleTypeDef hiwdg;   /* from CubeMX (enable IWDG in the .ioc) */

hk_app_t g_app;

/* GPS UART DMA + RX ring storage (normal RAM, DMA-accessible; not CCM). */
static uint8_t s_gps_dma[HK_GPS_DMA_BUF];
static uint8_t s_gps_rx[HK_GPS_RX_BUF];

/* ---------------------------------------------------------------- init ---- */

void hk_app_init(void)
{
    hk_time_init();
    hk_state_init();

    /* buses */
    hk_i2c_hw_init(&g_app.i2c1_hw, &g_app.i2c1, &hi2c1, 50,
                   GPIOB, GPIO_PIN_6,  GPIOB, GPIO_PIN_7);
    hk_i2c_hw_init(&g_app.i2c2_hw, &g_app.i2c2, &hi2c2, 50,
                   GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_11);
    hk_i2c_sw_init(&g_app.swi2c_hw, &g_app.swi2c,
                   HK_SWI2C_SCL_PORT, HK_SWI2C_SCL_PIN,
                   HK_SWI2C_SDA_PORT, HK_SWI2C_SDA_PIN, 5);

    hk_uart_dma_init(&g_app.gps_uart_dma, &g_app.gps_uart, &huart1,
                     s_gps_dma, HK_GPS_DMA_BUF, s_gps_rx, HK_GPS_RX_BUF, 100);
    hk_gps_init(&g_app.gps, &g_app.gps_uart);

    hk_battery_init(&g_app.batt, &hadc1, HK_BAT_ADC_CHANNEL,
                    HK_ADC_VREF_V, HK_ADC_FULL_SCALE, HK_BAT_DIVIDER_RATIO,
                    HK_BATT_CELLS, HK_BATT_EMA_ALPHA);

    hk_sensors_init(&g_app.sensors, &g_app.i2c1, &g_app.i2c2, &g_app.swi2c);

    /* actuators */
    hk_servo_init(&g_app.servo, &htim1, HK_SERVO_TIM_CHANNEL,
                  HK_SERVO_MIN_US, HK_SERVO_MAX_US);
    hk_buzzer_init(&g_app.buzzer, &htim3, HK_BUZZER_TIM_CHANNEL,
                   HK_BUZZER_TIMER_CLK_HZ);
    hk_fan_init(&g_app.fan1, HK_FAN1_GPIO_PORT, HK_FAN1_GPIO_PIN, true);
    hk_fan_init(&g_app.fan2, HK_FAN2_GPIO_PORT, HK_FAN2_GPIO_PIN, true);
    /* Fail-safe: start LOCKED. engaged_state polarity MUST be confirmed
     * against the solenoid mechanics (docs/ee-questions.md Q3). */
    hk_lock_init(&g_app.lock, HK_LOCK_GPIO_PORT, HK_LOCK_GPIO_PIN, GPIO_PIN_SET, true);

    hk_servo_set_angle(&g_app.servo, HK_SERVO_HOLD_DEG);

    hk_health_init(&hiwdg, HK_TASK_IMU | HK_TASK_ENV | HK_TASK_GPS | HK_TASK_CONTROL);

    HK_LOGI("app", "init complete, last reset = %s", hk_health_reset_reason());
}

/* -------------------------------------------------------------- tasks ----- */

static void task_imu(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    const float dt = (float)HK_IMU_PERIOD_MS / 1000.0f;
    for (;;) {
        hk_sensors_sample_imu(&g_app.sensors, dt);
        hk_health_kick(HK_TASK_IMU);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(HK_IMU_PERIOD_MS));
    }
}

static void task_env(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        hk_sensors_sample_env(&g_app.sensors);
        hk_health_kick(HK_TASK_ENV);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(HK_ENV_PERIOD_MS));
    }
}

static void task_gps(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        hk_gps_poll(&g_app.gps);
        hk_gps_fix_t fix;
        hk_gps_get_fix(&g_app.gps, &fix);
        hk_system_state_t *s = hk_state_lock();
        s->gps = fix;
        hk_state_unlock();
        hk_state_set_sensor_ok(HK_SENSOR_GPS, fix.valid);
        hk_health_kick(HK_TASK_GPS);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(HK_GPS_PERIOD_MS));
    }
}

static void task_control(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t   tick = 0;
    for (;;) {
        hk_system_state_t st;
        hk_state_get(&st);

        /* fan thermostat on the warmest enclosure reading */
        float t = st.temp_bmp_c;
        if (st.temp_sht1_c > t) { t = st.temp_sht1_c; }
        if (st.temp_sht2_c > t) { t = st.temp_sht2_c; }
        bool on = hk_fan_thermostat(t, HK_FAN_ON_ABOVE_C, HK_FAN_OFF_BELOW_C,
                                    hk_fan_is_on(&g_app.fan1));
        hk_fan_set(&g_app.fan1, on);
        hk_fan_set(&g_app.fan2, on);

        /* battery roughly every second */
        if ((tick % 10u) == 0u) {
            float v = 0, soc = 0;
            if (hk_battery_read(&g_app.batt, &v, &soc) == HK_OK) {
                hk_system_state_t *s = hk_state_lock();
                s->batt_voltage = v;
                s->batt_soc     = soc;
                hk_state_unlock();
                hk_state_set_sensor_ok(HK_SENSOR_BATT, true);
            }
        }

        hk_health_kick(HK_TASK_CONTROL);
        ++tick;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(HK_CONTROL_PERIOD_MS));
    }
}

static void task_health(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t missing = hk_health_service();
        if (missing != 0u) {
            HK_LOGW("health", "tasks not alive, mask=0x%02lX", (unsigned long)missing);
        }
        hk_system_state_t *s = hk_state_lock();
        s->uptime_ms = hk_millis();
        hk_state_unlock();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(HK_HEALTH_PERIOD_MS));
    }
}

void hk_app_start(void)
{
    xTaskCreate(task_imu,     "imu",     HK_STACK_IMU,     NULL, HK_PRIO_IMU,     NULL);
    xTaskCreate(task_env,     "env",     HK_STACK_ENV,     NULL, HK_PRIO_ENV,     NULL);
    xTaskCreate(task_gps,     "gps",     HK_STACK_GPS,     NULL, HK_PRIO_GPS,     NULL);
    xTaskCreate(task_control, "control", HK_STACK_CONTROL, NULL, HK_PRIO_CONTROL, NULL);
    xTaskCreate(task_health,  "health",  HK_STACK_HEALTH,  NULL, HK_PRIO_HEALTH,  NULL);
}

#endif /* !HK_HOST */
