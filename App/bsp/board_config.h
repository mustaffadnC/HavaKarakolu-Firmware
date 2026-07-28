#ifndef HK_BOARD_CONFIG_H
#define HK_BOARD_CONFIG_H

/*
 * Single source of truth for the board's pin map and peripheral assignments
 * (STM32F407VGTx, LQFP100).
 *
 * TWO board builds exist (2026-07 EE answers, docs/ee-questions.md):
 *
 *   HK_BOARD_SUKRU (default) -- Sukru's board, tried FIRST.
 *     SWD correctly on PA13/PA14 (ST-Link works). Buzzer PB14/TIM12_CH1,
 *     FAN2 PB13, NO battery sense (PC0 is NC), PB5/PB15 unused.
 *
 *   HK_BOARD_REV2A -- the first board (fallback if Sukru's fails).
 *     SWDIO/SWCLK MISWIRED to PB13/PB14 => flash via BOOT0 + UART bootloader
 *     (docs/bringup.md §3). Buzzer PB5/TIM3_CH2, FAN2 PB15, BAT_TEST on PC0.
 *
 * Select by defining HK_BOARD_REV2A as a compiler symbol; default is SUKRU.
 *
 * Common to both: GPS USART1 (PA9/PA10), I2C1 PB6/PB7 (BMI270 0x68 + BMP280
 * 0x76, 4.7K pullups), SHT4x_1 I2C2 (PB10/PB11), SHT4x_2 bit-bang (PB8/PB9),
 * SD on SPI1 (PA4 CS / PA5 SCK / PA6 MISO / PA7 MOSI, 5V Arduino module),
 * servo TIM1_CH1 PA8 (74AHCT1G125 -> 5V), solenoid PB0, BMI270 INT1 PC4,
 * HSE 8 MHz (EE answer S2).
 *
 * Solenoid (EE answer S3): normally-closed => DE-ENERGIZED = LOCKED (the
 * desired fail-safe). Energizing (~0.41 A) releases. PB0 low = locked.
 *
 * Target-only: includes the CubeMX/HAL headers. Host unit tests never include
 * this file (drivers depend on the bus interfaces, not the HAL).
 */

#if !defined(HK_HOST)

#include "main.h"   /* CubeMX-generated: GPIO defines + HAL handle externs */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Expected CubeMX peripheral handle names (rename here if different) ---- */
extern I2C_HandleTypeDef  hi2c1;    /* BMI270 + BMP280  (PB6/PB7)            */
extern I2C_HandleTypeDef  hi2c2;    /* SHT4x #1         (PB10/PB11)          */
extern UART_HandleTypeDef huart1;   /* GPS MAX-M10S     (PA9/PA10)           */
extern TIM_HandleTypeDef  htim1;    /* Servo PWM        (PA8  / TIM1_CH1)    */
extern SPI_HandleTypeDef  hspi1;    /* SD card module   (PA5/PA6/PA7)        */

#if defined(HK_BOARD_REV2A)
extern TIM_HandleTypeDef  htim3;    /* Buzzer           (PB5  / TIM3_CH2)    */
extern ADC_HandleTypeDef  hadc1;    /* BAT_TEST         (PC0  / ADC1_IN10)   */
#define HK_BUZZER_TIM_HANDLE    htim3
#define HK_BUZZER_TIM_CHANNEL   TIM_CHANNEL_2
#define HK_HAS_BAT_SENSE        1
#define HK_FAN2_GPIO_PIN        GPIO_PIN_15   /* PB15 */
#else /* HK_BOARD_SUKRU (default) */
extern TIM_HandleTypeDef  htim12;   /* Buzzer           (PB14 / TIM12_CH1)   */
#define HK_BUZZER_TIM_HANDLE    htim12
#define HK_BUZZER_TIM_CHANNEL   TIM_CHANNEL_1
#define HK_HAS_BAT_SENSE        0             /* PC0 is NC on this board */
#define HK_FAN2_GPIO_PIN        GPIO_PIN_13   /* PB13 */
#endif

/* ---- I2C 7-bit device addresses ---- */
#define HK_ADDR_BMI270          0x68   /* SDO=GND                            */
#define HK_ADDR_BMP280          0x76   /* SDO=GND, CSB=3.3V (I2C mode)       */
#define HK_ADDR_SHT4X           0x44   /* fixed; two units on separate buses */

/* ---- GPIO: solenoid trigger (PB0, net Solenoid_Tetik) ----
 * Low-side N-FET with gate pulldown. PB0 LOW = de-energized = LOCKED
 * (EE answer S3: normally closed). PB0 HIGH energizes (~0.41 A) = RELEASE.
 * The mission logic energizes only during the RELEASE window to save power
 * and keep the coil cool. */
#define HK_LOCK_GPIO_PORT       GPIOB
#define HK_LOCK_GPIO_PIN        GPIO_PIN_0

/* ---- GPIO: fan low-side drivers (12V fans, N-FET; EE answer S4) ----
 * No usable timer OC channel on these pins => on/off thermostat control. */
#define HK_FAN1_GPIO_PORT       GPIOB
#define HK_FAN1_GPIO_PIN        GPIO_PIN_12
#define HK_FAN2_GPIO_PORT       GPIOB
/* HK_FAN2_GPIO_PIN: variant-specific, defined above */

/* ---- Bit-banged I2C for the second SHT4x (PB8=SCL, PB9=SDA, open-drain) ---- */
#define HK_SWI2C_SCL_PORT       GPIOB
#define HK_SWI2C_SCL_PIN        GPIO_PIN_8
#define HK_SWI2C_SDA_PORT       GPIOB
#define HK_SWI2C_SDA_PIN        GPIO_PIN_9

/* ---- SD card (SPI1, software chip-select on PA4; 5V Arduino module with
 * onboard 1117 regulator -- EE answer S7) ---- */
#define HK_SD_CS_GPIO_PORT      GPIOA
#define HK_SD_CS_GPIO_PIN       GPIO_PIN_4

/* ---- BMI270 interrupt (PC4 -> EXTI4, INT1 per EE answer S10) ---- */
#define HK_BMI270_INT_GPIO_PORT GPIOC
#define HK_BMI270_INT_GPIO_PIN  GPIO_PIN_4

/* ---- Timer channels ---- */
#define HK_SERVO_TIM_CHANNEL    TIM_CHANNEL_1   /* htim1, PA8 (74AHCT1G125 -> 5V) */
/* HK_BUZZER_TIM_CHANNEL: variant-specific, defined above */

#if HK_HAS_BAT_SENSE
/* ---- ADC (REV2A only) ----
 * Battery sense divider R3=100k (top) / R22=10k (bottom), 1/8W (S11).
 * Vbat = Vadc * 11.0; 3S Li-ion assumed. Calibrate via CONFIG.INI. */
#define HK_BAT_ADC_CHANNEL      ADC_CHANNEL_10  /* PC0, net BAT_TEST */
#endif
#define HK_BAT_DIVIDER_RATIO    11.0f
#define HK_ADC_VREF_V           3.30f
#define HK_ADC_FULL_SCALE       4095.0f   /* 12-bit */

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_BOARD_CONFIG_H */
