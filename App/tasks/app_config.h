#ifndef HK_APP_CONFIG_H
#define HK_APP_CONFIG_H

/* ---- Task periods (ms) ---- */
#define HK_IMU_PERIOD_MS        10     /* 100 Hz */
#define HK_ENV_PERIOD_MS        500    /* 2 Hz   */
#define HK_GPS_PERIOD_MS        50
#define HK_CONTROL_PERIOD_MS    100
#define HK_HEALTH_PERIOD_MS     100
#define HK_STORAGE_PERIOD_MS    100

/* ---- Task priorities (higher = more urgent; idle = 0) ---- */
#define HK_PRIO_IMU             3
#define HK_PRIO_MISSION         3
#define HK_PRIO_ENV             2
#define HK_PRIO_GPS             2
#define HK_PRIO_CONTROL         2
#define HK_PRIO_STORAGE         1
#define HK_PRIO_HEALTH          1

/* ---- Task stack sizes (words) ---- */
#define HK_STACK_IMU            512
#define HK_STACK_ENV            512
#define HK_STACK_GPS            512
#define HK_STACK_CONTROL        384
#define HK_STACK_HEALTH         256
#define HK_STACK_STORAGE        768    /* FatFs path buffers on this stack */

/* ---- Storage / SD logging ---- */
#define HK_STORAGE_QUEUE_BYTES  4096   /* record queue (power of two) */
#define HK_SD_DATA_HZ           10500000u  /* 84 MHz / 8 */
#define HK_SD_INIT_TIMEOUT_MS   250
#define HK_FW_VERSION           0x0200 /* 2.0 = rev-2 board bring-up line */
#define HK_GPS_LOG_DIVIDER      20     /* gps task ticks per logged fix (1 Hz) */
#define HK_IMU_LOG_DECIM        4      /* keep 1-of-4 IMU records (25 Hz) */

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

/* ---- Servo pulse range (µs) ---- */
#define HK_SERVO_MIN_US         1000
#define HK_SERVO_MAX_US         2000
#define HK_SERVO_RELEASE_DEG    180.0f
#define HK_SERVO_HOLD_DEG       0.0f

/* ---- Buzzer timer input clock (TIM3 on APB1 -> 84 MHz) ---- */
#define HK_BUZZER_TIMER_CLK_HZ  84000000u

#endif /* HK_APP_CONFIG_H */
