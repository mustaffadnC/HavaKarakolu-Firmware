#ifndef HK_BOARD_CONFIG_H
#define HK_BOARD_CONFIG_H

/*
 * Single source of truth for the board's pin map and peripheral assignments,
 * derived directly from the ÇARGE schematic (STM32F405VGTx, LQFP100).
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
extern I2C_HandleTypeDef  hi2c1;    /* BMI270 + BMP581  (PB6/PB7)            */
extern I2C_HandleTypeDef  hi2c2;    /* SHT4x #1         (PB10/PB11)          */
extern UART_HandleTypeDef huart1;   /* GPS MAX-M10S     (PA9/PA10)           */
extern TIM_HandleTypeDef  htim1;    /* Servo PWM        (PA8  / TIM1_CH1)    */
extern TIM_HandleTypeDef  htim5;    /* WS2812 x2        (PA1/PA2 CH2/CH3)    */
extern TIM_HandleTypeDef  htim12;   /* Buzzer           (PB14 / TIM12_CH1)   */
extern ADC_HandleTypeDef  hadc1;    /* BAT_SENSE        (PC0  / ADC1_IN10)   */

/* ---- I2C 7-bit device addresses ---- */
#define HK_ADDR_BMI270          0x68   /* SDO=GND */
#define HK_ADDR_BMP581          0x46   /* SDO=GND */
#define HK_ADDR_SHT4X           0x44   /* fixed; two units on separate buses */

/* ---- GPIO: solenoid lock (PC13) ---- */
#define HK_LOCK_GPIO_PORT       GPIOC
#define HK_LOCK_GPIO_PIN        GPIO_PIN_13

/* ---- GPIO: fan low-side drivers (PB12/PB13) ---- */
#define HK_FAN1_GPIO_PORT       GPIOB
#define HK_FAN1_GPIO_PIN        GPIO_PIN_12
#define HK_FAN2_GPIO_PORT       GPIOB
#define HK_FAN2_GPIO_PIN        GPIO_PIN_13

/* ---- Bit-banged I2C for the second SHT4x (PB8=SCL, PB9=SDA, open-drain) ---- */
#define HK_SWI2C_SCL_PORT       GPIOB
#define HK_SWI2C_SCL_PIN        GPIO_PIN_8
#define HK_SWI2C_SDA_PORT       GPIOB
#define HK_SWI2C_SDA_PIN        GPIO_PIN_9

/* ---- Timer channels ---- */
#define HK_SERVO_TIM_CHANNEL    TIM_CHANNEL_1   /* htim1, PA8  */
#define HK_WS2812_0_CHANNEL     TIM_CHANNEL_2   /* htim5, PA1  (D6) */
#define HK_WS2812_1_CHANNEL     TIM_CHANNEL_3   /* htim5, PA2  (D7) */
#define HK_BUZZER_TIM_CHANNEL   TIM_CHANNEL_1   /* htim12, PB14 */

/* ---- ADC ---- */
#define HK_BAT_ADC_CHANNEL      ADC_CHANNEL_10  /* PC0 */

/*
 * Battery sense scaling: divider R15=100k (top) / R16=22k (bottom).
 * Vbat = Vadc * (100k + 22k) / 22k = Vadc * 5.5454...
 * NOTE: confirm exact resistor values + Li-ion cell count (3S assumed).
 */
#define HK_BAT_DIVIDER_RATIO    5.54545f
#define HK_ADC_VREF_V           3.30f
#define HK_ADC_FULL_SCALE       4095.0f   /* 12-bit */

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_BOARD_CONFIG_H */
