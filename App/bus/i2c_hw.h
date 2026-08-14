#ifndef HK_I2C_HW_H
#define HK_I2C_HW_H

#include "bus/i2c_bus_if.h"

#if !defined(HK_HOST)

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32 HAL-backed I2C bus. Thread-safe via a FreeRTOS mutex; register reads
 * use HAL_I2C_Mem_Read (repeated-START). recover() performs the standard
 * 9-clock SCL pulse bus-clear sequence, then re-inits the peripheral.
 *
 * SCL/SDA GPIO are needed only for recover() — pass the bus pins
 * (I2C1: PB6/PB7, I2C2: PB10/PB11).
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    void              *mutex;       /* SemaphoreHandle_t (opaque to avoid RTOS in header) */
    uint32_t           timeout_ms;
    GPIO_TypeDef      *scl_port;
    uint16_t           scl_pin;
    GPIO_TypeDef      *sda_port;
    uint16_t           sda_pin;
} hk_i2c_hw_t;

/* Initialise `self` and fill the abstract `bus` with function pointers.
 * Creates the mutex. Returns HK_OK or HK_ERR. */
hk_status_t hk_i2c_hw_init(hk_i2c_hw_t *self, hk_i2c_bus_t *bus,
                           I2C_HandleTypeDef *hi2c, uint32_t timeout_ms,
                           GPIO_TypeDef *scl_port, uint16_t scl_pin,
                           GPIO_TypeDef *sda_port, uint16_t sda_pin);


hk_status_t hk_i2c_hw_rtos_init(hk_i2c_hw_t *self);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */
#endif /* HK_I2C_HW_H */
