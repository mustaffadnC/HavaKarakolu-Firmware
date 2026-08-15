#ifndef HK_APP_CONFIG_H
#define HK_APP_CONFIG_H

/* ---- Task periods (ms) ---- */
#define HK_IMU_PERIOD_MS        10     /* 100 Hz */
#define HK_ENV_PERIOD_MS        500    /* 2 Hz   */
#define HK_GPS_PERIOD_MS        50
#define HK_CONTROL_PERIOD_MS    100
#define HK_HEALTH_PERIOD_MS     100
#define HK_STORAGE_PERIOD_MS    100
#define HK_MISSION_PERIOD_MS    50

/* ---- Health reporting ----
 * A task whose period is longer than HK_HEALTH_PERIOD_MS is legitimately
 * missing on most health passes: ENV runs at 500 ms against a 100 ms pass, so
 * hk_health_service() reports mask=0x02 on four passes out of five. Warning on
 * every pass buried the 2 KB SRAM log ring under ~8 identical lines per second
 * and evicted the boot record (docs/DEVIR-TESLIM.md 15.3). Only a mask that
 * survives this many consecutive passes is worth reporting: 10 x 100 ms = 1 s,
 * comfortably longer than the slowest monitored task.
 * Reporting only -- the IWDG refresh policy in hk_health_service() is
 * unchanged and still demands every expected task on every refresh. */
#define HK_HEALTH_WARN_PASSES   10u

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
#define HK_STACK_MISSION        384

/* ---- Storage / SD logging ---- */
#define HK_STORAGE_QUEUE_BYTES  4096   /* record queue (power of two) */
/* SPI clock for the data phase. Was 84 MHz / 8 = 10.5 MHz, which this board's
 * 5 V Arduino-style module could not sustain: the first f_mount succeeded (so
 * the card and its FAT filesystem are fine) but the very next write failed and
 * every retry then came back FR_DISK_ERR -- see docs/DEVIR-TESLIM.md 8.8.
 * 84 MHz / 32 leaves plenty of margin for the module's level shifter and the
 * jumper wiring. Raise it again only with a logic analyser on the bus. */
#define HK_SD_DATA_HZ           2625000u   /* 84 MHz / 32 */
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

/* ---- Buzzer timer input clock (TIM12 or TIM3, both APB1 -> 84 MHz) ---- */
#define HK_BUZZER_TIMER_CLK_HZ  84000000u

/* ---- TEMPORARY bring-up diagnostic -- set back to 0 for flight builds ----
 * With this at 1 the buzzer holds a continuous tone, started at the very top
 * of hk_app_init() before any bus or sensor is touched, and the mission
 * buzzer patterns are suppressed. That turns the buzzer into an audible
 * "MCU is powered and the clock tree came up" indicator which survives a
 * hang later in init -- used to tell whether the board boots on its own 3V3
 * rail. See docs/DEVIR-TESLIM.md 8.3. */
#define HK_DIAG_BUZZER_ALWAYS   0
#define HK_DIAG_BUZZER_FREQ_HZ  2000u

#endif /* HK_APP_CONFIG_H */
