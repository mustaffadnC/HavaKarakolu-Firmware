#ifndef HK_I2C_SW_H
#define HK_I2C_SW_H

#include "bus/i2c_bus_if.h"

#if !defined(HK_HOST)

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Software (bit-banged) I2C master for the second SHT4x (PB8=SCL, PB9=SDA).
 * Pins must be configured as open-drain outputs; external 10k pull-ups
 * (R30/R31) are present on the board. Uses DWT cycle counter for µs timing,
 * supports clock stretching. ~100 kHz with half_period_us = 5.
 */
typedef struct {
    GPIO_TypeDef *scl_port;
    uint16_t      scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t      sda_pin;
    uint32_t      half_period_us;
    void         *mutex;          /* SemaphoreHandle_t */
} hk_i2c_sw_t;

hk_status_t hk_i2c_sw_init(hk_i2c_sw_t *self, hk_i2c_bus_t *bus,
                           GPIO_TypeDef *scl_port, uint16_t scl_pin,
                           GPIO_TypeDef *sda_port, uint16_t sda_pin,
                           uint32_t half_period_us);


hk_status_t hk_i2c_sw_rtos_init(hk_i2c_sw_t *self);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */
#endif /* HK_I2C_SW_H */
