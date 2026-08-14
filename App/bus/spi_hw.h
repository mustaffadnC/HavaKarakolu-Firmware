#ifndef HK_SPI_HW_H
#define HK_SPI_HW_H

#include "bus/spi_if.h"

#if !defined(HK_HOST)

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32 HAL-backed SPI bus with a software chip-select GPIO (PA4 for the SD
 * card). Thread-safe via a FreeRTOS mutex around each transfer. set_speed()
 * switches the baud-rate prescaler at runtime (SD init at <=400 kHz, data
 * phase fast); pass the peripheral input clock so the divider can be chosen
 * (SPI1 is on APB2 = 84 MHz).
 */
typedef struct {
    SPI_HandleTypeDef *hspi;
    void              *mutex;        /* SemaphoreHandle_t (opaque) */
    uint32_t           timeout_ms;
    uint32_t           pclk_hz;      /* SPI kernel clock (APB2 for SPI1) */
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
} hk_spi_hw_t;

/* Initialise `self`, fill the abstract `bus`, create the mutex and release
 * chip-select. Returns HK_OK or HK_ERR. */
hk_status_t hk_spi_hw_init(hk_spi_hw_t *self, hk_spi_bus_t *bus,
                           SPI_HandleTypeDef *hspi, uint32_t timeout_ms,
                           uint32_t pclk_hz,
                           GPIO_TypeDef *cs_port, uint16_t cs_pin);


hk_status_t hk_spi_hw_rtos_init(hk_spi_hw_t *self);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */
#endif /* HK_SPI_HW_H */
