#ifndef HK_BOARD_CONFIG_H
#define HK_BOARD_CONFIG_H

/*
 * Single source of truth for the board's pin map and peripheral assignments,
 * derived directly from the ÇARGE rev-2 schematic (STM32F405VGTx, LQFP100).
 *
 * Rev-2 changes vs rev-1:
 *   - Pressure sensor is BMP280 @0x76 (was BMP581 @0x46), still on I2C1.
 *   - Buzzer moved PB14/TIM12_CH1 -> PB5/TIM3_CH2.
 *   - Fan2 moved PB13 -> PB15. Solenoid moved PC13 -> PB0.
 *   - WS2812 status LEDs removed (PA1/PA2 are NC on rev-2).
 *   - NEW: SD card on SPI1 (PA4 CS / PA5 SCK / PA6 MISO / PA7 MOSI), J3 header.
 *   - NEW: BMI270 INT1 on PC4 (EXTI4, optional data-ready).
 *   - Battery divider is now 100k/10k (x11), was 100k/22k.
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
extern TIM_HandleTypeDef  htim3;    /* Buzzer           (PB5  / TIM3_CH2)    */
extern SPI_HandleTypeDef  hspi1;    /* SD card module   (PA5/PA6/PA7)        */
extern ADC_HandleTypeDef  hadc1;    /* BAT_TEST         (PC0  / ADC1_IN10)   */

/* ---- I2C 7-bit device addresses ---- */
#define HK_ADDR_BMI270          0x68   /* SDO=GND                            */
#define HK_ADDR_BMP280          0x76   /* SDO=GND, CSB=3.3V (I2C mode)       */
#define HK_ADDR_SHT4X           0x44   /* fixed; two units on separate buses */

/* ---- GPIO: solenoid trigger (PB0, net Solenoid_Tetik) ----
 * AO3406 low-side driver with 10K gate pulldown (R15):
 * MCU reset / PB0 LOW  => MOSFET off => solenoid DE-ENERGIZED.
 * Whether de-energized means LOCKED (desired fail-safe) or UNLOCKED is an
 * open hardware question -- see docs/ee-questions.md Q3 before first arming. */
#define HK_LOCK_GPIO_PORT       GPIOB
#define HK_LOCK_GPIO_PIN        GPIO_PIN_0

/* ---- GPIO: fan low-side drivers (PB12 / PB15, AO3400A + 100R gate) ----
 * Neither pin has a usable timer OC channel => on/off thermostat control. */
#define HK_FAN1_GPIO_PORT       GPIOB
#define HK_FAN1_GPIO_PIN        GPIO_PIN_12
#define HK_FAN2_GPIO_PORT       GPIOB
#define HK_FAN2_GPIO_PIN        GPIO_PIN_15

/* ---- Bit-banged I2C for the second SHT4x (PB8=SCL, PB9=SDA, open-drain) ---- */
#define HK_SWI2C_SCL_PORT       GPIOB
#define HK_SWI2C_SCL_PIN        GPIO_PIN_8
#define HK_SWI2C_SDA_PORT       GPIOB
#define HK_SWI2C_SDA_PIN        GPIO_PIN_9

/* ---- SD card (SPI1, software chip-select on PA4; J3 header, 5V module) ---- */
#define HK_SD_CS_GPIO_PORT      GPIOA
#define HK_SD_CS_GPIO_PIN       GPIO_PIN_4

/* ---- BMI270 interrupt (PC4 -> EXTI4; optional data-ready path) ---- */
#define HK_BMI270_INT_GPIO_PORT GPIOC
#define HK_BMI270_INT_GPIO_PIN  GPIO_PIN_4

/* ---- Timer channels ---- */
#define HK_SERVO_TIM_CHANNEL    TIM_CHANNEL_1   /* htim1, PA8 (74AHCT1G125 -> 5V) */
#define HK_BUZZER_TIM_CHANNEL   TIM_CHANNEL_2   /* htim3, PB5                      */

/* ---- ADC ---- */
#define HK_BAT_ADC_CHANNEL      ADC_CHANNEL_10  /* PC0, net BAT_TEST */

/*
 * Battery sense scaling: divider R3=100k (top) / R22=10k (bottom).
 * Vbat = Vadc * (100k + 10k) / 10k = Vadc * 11.0
 * 3S Li-ion assumed (9.0 .. 12.6V -> 0.82 .. 1.15V at the ADC pin).
 * Confirm resistor tolerance (docs/ee-questions.md Q11); calibrate at bring-up.
 */
#define HK_BAT_DIVIDER_RATIO    11.0f
#define HK_ADC_VREF_V           3.30f
#define HK_ADC_FULL_SCALE       4095.0f   /* 12-bit */

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_BOARD_CONFIG_H */
