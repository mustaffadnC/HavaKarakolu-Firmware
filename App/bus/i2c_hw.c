#include "bus/i2c_hw.h"

#if !defined(HK_HOST)

#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

/* ---- helpers ---- */

static hk_status_t map_hal(HAL_StatusTypeDef hs)
{
    switch (hs) {
    case HAL_OK:      return HK_OK;
    case HAL_TIMEOUT: return HK_ERR_TIMEOUT;
    case HAL_BUSY:    return HK_ERR_BUSY;
    default:          return HK_ERR_IO;
    }
}

static bool lock(hk_i2c_hw_t *s)
{
    if (s->mutex == NULL) {
        return true; /* pre-scheduler use */
    }
    return xSemaphoreTake((SemaphoreHandle_t)s->mutex, pdMS_TO_TICKS(s->timeout_ms + 50)) == pdTRUE;
}

static void unlock(hk_i2c_hw_t *s)
{
    if (s->mutex != NULL) {
        (void)xSemaphoreGive((SemaphoreHandle_t)s->mutex);
    }
}

/* ---- i2c_bus_if implementation ---- */

static hk_status_t op_write(void *ctx, uint8_t addr7, const uint8_t *data, size_t len)
{
    hk_i2c_hw_t *s = (hk_i2c_hw_t *)ctx;
    if (!lock(s)) {
        return HK_ERR_BUSY;
    }
    HAL_StatusTypeDef hs = HAL_I2C_Master_Transmit(
        s->hi2c, (uint16_t)(addr7 << 1), (uint8_t *)data, (uint16_t)len, s->timeout_ms);
    unlock(s);
    return map_hal(hs);
}

static hk_status_t op_read(void *ctx, uint8_t addr7, uint8_t *buf, size_t len)
{
    hk_i2c_hw_t *s = (hk_i2c_hw_t *)ctx;
    if (!lock(s)) {
        return HK_ERR_BUSY;
    }
    HAL_StatusTypeDef hs = HAL_I2C_Master_Receive(
        s->hi2c, (uint16_t)(addr7 << 1), buf, (uint16_t)len, s->timeout_ms);
    unlock(s);
    return map_hal(hs);
}

static hk_status_t op_write_read(void *ctx, uint8_t addr7,
                                 const uint8_t *wbuf, size_t wlen,
                                 uint8_t *rbuf, size_t rlen)
{
    hk_i2c_hw_t *s = (hk_i2c_hw_t *)ctx;
    HAL_StatusTypeDef hs;

    if (!lock(s)) {
        return HK_ERR_BUSY;
    }

    if (wlen == 1 || wlen == 2) {
        /* Register read with repeated-START (no STOP between). */
        uint16_t memaddr = (wlen == 1) ? wbuf[0]
                                       : (uint16_t)((wbuf[0] << 8) | wbuf[1]);
        uint16_t memsize = (wlen == 1) ? I2C_MEMADD_SIZE_8BIT
                                       : I2C_MEMADD_SIZE_16BIT;
        hs = HAL_I2C_Mem_Read(s->hi2c, (uint16_t)(addr7 << 1), memaddr, memsize,
                              rbuf, (uint16_t)rlen, s->timeout_ms);
    } else {
        /* Fallback: write then read (STOP in between). */
        hs = HAL_I2C_Master_Transmit(s->hi2c, (uint16_t)(addr7 << 1),
                                     (uint8_t *)wbuf, (uint16_t)wlen, s->timeout_ms);
        if (hs == HAL_OK) {
            hs = HAL_I2C_Master_Receive(s->hi2c, (uint16_t)(addr7 << 1),
                                        rbuf, (uint16_t)rlen, s->timeout_ms);
        }
    }

    unlock(s);
    return map_hal(hs);
}

static hk_status_t op_probe(void *ctx, uint8_t addr7)
{
    hk_i2c_hw_t *s = (hk_i2c_hw_t *)ctx;
    if (!lock(s)) {
        return HK_ERR_BUSY;
    }
    HAL_StatusTypeDef hs = HAL_I2C_IsDeviceReady(
        s->hi2c, (uint16_t)(addr7 << 1), 2, s->timeout_ms);
    unlock(s);
    return (hs == HAL_OK) ? HK_OK : HK_ERR_NACK;
}

/* Standard I2C bus-clear: drive 9 SCL pulses while SDA is released, then a
 * STOP, finally re-init the peripheral. Clears a slave stuck mid-byte. */
static void op_recover(void *ctx)
{
    hk_i2c_hw_t *s = (hk_i2c_hw_t *)ctx;
    if (s->scl_port == NULL || s->sda_port == NULL) {
        (void)HAL_I2C_DeInit(s->hi2c);
        (void)HAL_I2C_Init(s->hi2c);
        return;
    }

    (void)lock(s);

    (void)HAL_I2C_DeInit(s->hi2c);

    GPIO_InitTypeDef gi = {0};
    gi.Mode  = GPIO_MODE_OUTPUT_OD;
    gi.Pull  = GPIO_PULLUP;
    gi.Speed = GPIO_SPEED_FREQ_LOW;

    gi.Pin = s->scl_pin;
    HAL_GPIO_Init(s->scl_port, &gi);
    gi.Pin = s->sda_pin;
    HAL_GPIO_Init(s->sda_port, &gi);

    HAL_GPIO_WritePin(s->sda_port, s->sda_pin, GPIO_PIN_SET); /* release SDA */
    for (int i = 0; i < 9; ++i) {
        HAL_GPIO_WritePin(s->scl_port, s->scl_pin, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(s->scl_port, s->scl_pin, GPIO_PIN_SET);
        HAL_Delay(1);
    }
    /* STOP condition: SDA low->high while SCL high. */
    HAL_GPIO_WritePin(s->sda_port, s->sda_pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(s->scl_port, s->scl_pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(s->sda_port, s->sda_pin, GPIO_PIN_SET);
    HAL_Delay(1);

    (void)HAL_I2C_Init(s->hi2c);

    unlock(s);
}

hk_status_t hk_i2c_hw_init(hk_i2c_hw_t *self, hk_i2c_bus_t *bus,
                           I2C_HandleTypeDef *hi2c, uint32_t timeout_ms,
                           GPIO_TypeDef *scl_port, uint16_t scl_pin,
                           GPIO_TypeDef *sda_port, uint16_t sda_pin)
{
    if (self == NULL || bus == NULL || hi2c == NULL) {
        return HK_ERR_PARAM;
    }
    self->hi2c       = hi2c;
    self->timeout_ms = (timeout_ms == 0) ? 50U : timeout_ms;
    self->scl_port   = scl_port;
    self->scl_pin    = scl_pin;
    self->sda_port   = sda_port;
    self->sda_pin    = sda_pin;
    self->mutex      = xSemaphoreCreateMutex();
    if (self->mutex == NULL) {
        return HK_ERR;
    }

    bus->ctx        = self;
    bus->write      = op_write;
    bus->read       = op_read;
    bus->write_read = op_write_read;
    bus->probe      = op_probe;
    bus->recover    = op_recover;
    return HK_OK;
}

#endif /* !HK_HOST */
