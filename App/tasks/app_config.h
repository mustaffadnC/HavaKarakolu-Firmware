#ifndef HK_APP_CONFIG_H
#define HK_APP_CONFIG_H

/* ---- Task periods (ms) ---- */
#define HK_IMU_PERIOD_MS        10     /* 100 Hz */
#define HK_ENV_PERIOD_MS        500    /* 2 Hz   */
#define HK_GPS_PERIOD_MS        50
#define HK_CONTROL_PERIOD_MS    100
#define HK_HEALTH_PERIOD_MS     100

/* ---- Task priorities (higher = more urgent; idle = 0) ---- */
#define HK_PRIO_IMU             3
#define HK_PRIO_MISSION         3
#define HK_PRIO_ENV             2
#define HK_PRIO_GPS             2
#define HK_PRIO_CONTROL         2
#define HK_PRIO_TELEM           1
#define HK_PRIO_HEALTH          1

/* ---- Task stack sizes (words) ---- */
#define HK_STACK_IMU            512
#define HK_STACK_ENV            512
#define HK_STACK_GPS            512
#define HK_STACK_CONTROL        384
#define HK_STACK_HEALTH         256

/* ---- GPS UART buffers (rx capacity MUST be power of two) ---- */
#define HK_GPS_DMA_BUF          256
#define HK_GPS_RX_BUF           512

/* ---- Fan thermostat (°C), sensor aspiration / enclosure cooling ---- */
#define HK_FAN_ON_ABOVE_C       40.0f
#define HK_FAN_OFF_BELOW_C      35.0f

/* ---- Battery (confirm cell count from hardware; 3S assumed) ---- */
#define HK_BATT_CELLS           3
#define HK_BATT_EMA_ALPHA       0.2f
#define HK_BATT_LOW_SOC         0.15f   /* enter power-save below this */

/* ---- WS2812 timing ticks (TIM5 @84 MHz, ARR=104, 1.25 µs bit) ---- */
#define HK_WS2812_HI_TICKS      58
#define HK_WS2812_LO_TICKS      29

/* ---- Servo pulse range (µs) ---- */
#define HK_SERVO_MIN_US         1000
#define HK_SERVO_MAX_US         2000
#define HK_SERVO_RELEASE_DEG    180.0f
#define HK_SERVO_HOLD_DEG       0.0f

/* ---- Buzzer timer input clock (TIM12 on APB1 -> 84 MHz) ---- */
#define HK_BUZZER_TIMER_CLK_HZ  84000000u

#endif /* HK_APP_CONFIG_H */
