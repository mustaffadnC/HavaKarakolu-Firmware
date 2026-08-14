#include "bus/spi_hw.h"

#include <stddef.h>

#if !defined(HK_HOST)

#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

static hk_status_t map_hal(HAL_StatusTypeDef hs)
{
    switch (hs) {
    case HAL_OK:      return HK_OK;
    case HAL_TIMEOUT: return HK_ERR_TIMEOUT;
    case HAL_BUSY:    return HK_ERR_BUSY;
    default:          return HK_ERR_IO;
    }
}

static bool lock(hk_spi_hw_t *s)
{
    if (s->mutex == NULL) {
        return true; /* pre-scheduler use */
    }
    return xSemaphoreTake((SemaphoreHandle_t)s->mutex,
                          pdMS_TO_TICKS(s->timeout_ms + 50)) == pdTRUE;
}

static void unlock(hk_spi_hw_t *s)
{
    if (s->mutex != NULL) {
        (void)xSemaphoreGive((SemaphoreHandle_t)s->mutex);
    }
}

/* ---- spi_if implementation ---- */

static hk_status_t op_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    hk_spi_hw_t *s = (hk_spi_hw_t *)ctx;
    if (len == 0) {
        return HK_OK;
    }
    if (!lock(s)) {
        return HK_ERR_BUSY;
    }

    HAL_StatusTypeDef hs;
    if (tx != NULL && rx != NULL) {
        hs = HAL_SPI_TransmitReceive(s->hspi, (uint8_t *)tx, rx,
                                     (uint16_t)len, s->timeout_ms);
    } else if (tx != NULL) {
        hs = HAL_SPI_Transmit(s->hspi, (uint8_t *)tx, (uint16_t)len,
                              s->timeout_ms);
    } else if (rx != NULL) {
        /* SD requires 0xFF fill on MOSI while reading; chunked fixed buffer. */
        uint8_t fill[16];
        for (size_t i = 0; i < sizeof(fill); ++i) { fill[i] = 0xFF; }
        hs = HAL_OK;
        size_t done = 0;
        while (done < len && hs == HAL_OK) {
            uint16_t n = (uint16_t)(((len - done) < sizeof(fill))
                                        ? (len - done) : sizeof(fill));
            hs = HAL_SPI_TransmitReceive(s->hspi, fill, rx + done, n,
                                         s->timeout_ms);
            done += n;
        }
    } else {
        unlock(s);
        return HK_ERR_PARAM;
    }

    unlock(s);
    return map_hal(hs);
}

static hk_status_t op_set_speed(void *ctx, uint32_t hz)
{
    hk_spi_hw_t *s = (hk_spi_hw_t *)ctx;
    if (hz == 0) {
        return HK_ERR_PARAM;
    }
    if (!lock(s)) {
        return HK_ERR_BUSY;
    }

    /* Pick the smallest prescaler whose rate is <= hz (prescalers 2..256). */
    static const uint32_t presc_reg[8] = {
        SPI_BAUDRATEPRESCALER_2,   SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8,   SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32,  SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256,
    };
    uint32_t div = 2;
    unsigned  idx = 0;
    while (idx < 7u && (s->pclk_hz / div) > hz) {
        div <<= 1;
        ++idx;
    }

    (void)HAL_SPI_DeInit(s->hspi);
    s->hspi->Init.BaudRatePrescaler = presc_reg[idx];
    HAL_StatusTypeDef hs = HAL_SPI_Init(s->hspi);

    unlock(s);
    return map_hal(hs);
}

static void op_cs(void *ctx, bool assert_cs)
{
    hk_spi_hw_t *s = (hk_spi_hw_t *)ctx;
    /* Active-low chip select. */
    HAL_GPIO_WritePin(s->cs_port, s->cs_pin,
                      assert_cs ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

hk_status_t hk_spi_hw_init(hk_spi_hw_t *self, hk_spi_bus_t *bus,
                           SPI_HandleTypeDef *hspi, uint32_t timeout_ms,
                           uint32_t pclk_hz,
                           GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    if (self == NULL || bus == NULL || hspi == NULL ||
        cs_port == NULL || pclk_hz == 0) {
        return HK_ERR_PARAM;
    }
    self->hspi       = hspi;
    self->timeout_ms = (timeout_ms == 0) ? 250U : timeout_ms;
    self->pclk_hz    = pclk_hz;
    self->cs_port    = cs_port;
    self->cs_pin     = cs_pin;
    self->mutex      = NULL;   /* created by hk_spi_hw_rtos_init() */

    bus->ctx       = self;
    bus->xfer      = op_xfer;
    bus->set_speed = op_set_speed;
    bus->cs        = op_cs;

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);   /* CS released */
    return HK_OK;
}


/* See hk_i2c_hw_rtos_init(): created just before the scheduler starts. */
hk_status_t hk_spi_hw_rtos_init(hk_spi_hw_t *self)
{
    if (self == NULL) { return HK_ERR_PARAM; }
    self->mutex = xSemaphoreCreateMutex();
    return (self->mutex == NULL) ? HK_ERR : HK_OK;
}

#endif /* !HK_HOST */
