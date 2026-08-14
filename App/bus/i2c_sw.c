#include "bus/i2c_sw.h"

#include <stddef.h>

#if !defined(HK_HOST)

#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

/* ---- DWT-based microsecond delay ---- */

static void dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < cycles) {
        /* busy-wait */
    }
}

/* ---- line primitives (open-drain) ---- */

static inline void scl_release(hk_i2c_sw_t *s) { HAL_GPIO_WritePin(s->scl_port, s->scl_pin, GPIO_PIN_SET); }
static inline void scl_low(hk_i2c_sw_t *s)     { HAL_GPIO_WritePin(s->scl_port, s->scl_pin, GPIO_PIN_RESET); }
static inline void sda_release(hk_i2c_sw_t *s) { HAL_GPIO_WritePin(s->sda_port, s->sda_pin, GPIO_PIN_SET); }
static inline void sda_low(hk_i2c_sw_t *s)     { HAL_GPIO_WritePin(s->sda_port, s->sda_pin, GPIO_PIN_RESET); }
static inline int  sda_read(hk_i2c_sw_t *s)    { return HAL_GPIO_ReadPin(s->sda_port, s->sda_pin) == GPIO_PIN_SET ? 1 : 0; }

/* Release SCL and wait until it actually reads high (clock stretching). */
static bool scl_release_wait(hk_i2c_sw_t *s)
{
    scl_release(s);
    delay_us(s->half_period_us);
    uint32_t guard = 0;
    while (HAL_GPIO_ReadPin(s->scl_port, s->scl_pin) == GPIO_PIN_RESET) {
        delay_us(s->half_period_us);
        if (++guard > 1000U) {  /* ~ up to several ms */
            return false;       /* stuck low */
        }
    }
    return true;
}

/* ---- I2C primitives ---- */

static void i2c_start(hk_i2c_sw_t *s)
{
    sda_release(s);
    (void)scl_release_wait(s);
    sda_low(s);                 /* SDA high->low while SCL high = START */
    delay_us(s->half_period_us);
    scl_low(s);
    delay_us(s->half_period_us);
}

static void i2c_stop(hk_i2c_sw_t *s)
{
    sda_low(s);
    delay_us(s->half_period_us);
    (void)scl_release_wait(s);
    sda_release(s);             /* SDA low->high while SCL high = STOP */
    delay_us(s->half_period_us);
}

/* Returns true if ACK received. */
static bool i2c_write_byte(hk_i2c_sw_t *s, uint8_t b)
{
    for (int i = 0; i < 8; ++i) {
        if (b & 0x80U) { sda_release(s); } else { sda_low(s); }
        b <<= 1;
        delay_us(s->half_period_us);
        if (!scl_release_wait(s)) { return false; }
        scl_low(s);
        delay_us(s->half_period_us);
    }
    /* ACK clock */
    sda_release(s);
    delay_us(s->half_period_us);
    if (!scl_release_wait(s)) { return false; }
    int ack = (sda_read(s) == 0);   /* slave pulls low */
    scl_low(s);
    delay_us(s->half_period_us);
    return ack;
}

static uint8_t i2c_read_byte(hk_i2c_sw_t *s, bool ack)
{
    uint8_t b = 0;
    sda_release(s);
    for (int i = 0; i < 8; ++i) {
        b <<= 1;
        if (!scl_release_wait(s)) { return b; }
        b |= (uint8_t)sda_read(s);
        scl_low(s);
        delay_us(s->half_period_us);
    }
    /* send ACK/NACK */
    if (ack) { sda_low(s); } else { sda_release(s); }
    delay_us(s->half_period_us);
    (void)scl_release_wait(s);
    scl_low(s);
    delay_us(s->half_period_us);
    sda_release(s);
    return b;
}

/* ---- mutex ---- */

static bool lock(hk_i2c_sw_t *s)
{
    if (s->mutex == NULL) { return true; }
    return xSemaphoreTake((SemaphoreHandle_t)s->mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}
static void unlock(hk_i2c_sw_t *s)
{
    if (s->mutex != NULL) { (void)xSemaphoreGive((SemaphoreHandle_t)s->mutex); }
}

/* ---- i2c_bus_if implementation ---- */

static hk_status_t op_write(void *ctx, uint8_t addr7, const uint8_t *data, size_t len)
{
    hk_i2c_sw_t *s = (hk_i2c_sw_t *)ctx;
    if (!lock(s)) { return HK_ERR_BUSY; }
    hk_status_t st = HK_OK;
    i2c_start(s);
    if (!i2c_write_byte(s, (uint8_t)(addr7 << 1))) { st = HK_ERR_NACK; goto done; }
    for (size_t i = 0; i < len; ++i) {
        if (!i2c_write_byte(s, data[i])) { st = HK_ERR_NACK; goto done; }
    }
done:
    i2c_stop(s);
    unlock(s);
    return st;
}

static hk_status_t op_read(void *ctx, uint8_t addr7, uint8_t *buf, size_t len)
{
    hk_i2c_sw_t *s = (hk_i2c_sw_t *)ctx;
    if (!lock(s)) { return HK_ERR_BUSY; }
    hk_status_t st = HK_OK;
    i2c_start(s);
    if (!i2c_write_byte(s, (uint8_t)((addr7 << 1) | 1U))) { st = HK_ERR_NACK; goto done; }
    for (size_t i = 0; i < len; ++i) {
        buf[i] = i2c_read_byte(s, (i + 1U) < len);  /* ACK all but last */
    }
done:
    i2c_stop(s);
    unlock(s);
    return st;
}

static hk_status_t op_write_read(void *ctx, uint8_t addr7,
                                 const uint8_t *wbuf, size_t wlen,
                                 uint8_t *rbuf, size_t rlen)
{
    hk_i2c_sw_t *s = (hk_i2c_sw_t *)ctx;
    if (!lock(s)) { return HK_ERR_BUSY; }
    hk_status_t st = HK_OK;

    i2c_start(s);
    if (!i2c_write_byte(s, (uint8_t)(addr7 << 1))) { st = HK_ERR_NACK; goto done; }
    for (size_t i = 0; i < wlen; ++i) {
        if (!i2c_write_byte(s, wbuf[i])) { st = HK_ERR_NACK; goto done; }
    }
    i2c_start(s);  /* repeated START */
    if (!i2c_write_byte(s, (uint8_t)((addr7 << 1) | 1U))) { st = HK_ERR_NACK; goto done; }
    for (size_t i = 0; i < rlen; ++i) {
        rbuf[i] = i2c_read_byte(s, (i + 1U) < rlen);
    }
done:
    i2c_stop(s);
    unlock(s);
    return st;
}

static hk_status_t op_probe(void *ctx, uint8_t addr7)
{
    hk_i2c_sw_t *s = (hk_i2c_sw_t *)ctx;
    if (!lock(s)) { return HK_ERR_BUSY; }
    i2c_start(s);
    bool ack = i2c_write_byte(s, (uint8_t)(addr7 << 1));
    i2c_stop(s);
    unlock(s);
    return ack ? HK_OK : HK_ERR_NACK;
}

hk_status_t hk_i2c_sw_init(hk_i2c_sw_t *self, hk_i2c_bus_t *bus,
                           GPIO_TypeDef *scl_port, uint16_t scl_pin,
                           GPIO_TypeDef *sda_port, uint16_t sda_pin,
                           uint32_t half_period_us)
{
    if (self == NULL || bus == NULL) { return HK_ERR_PARAM; }

    dwt_enable();

    self->scl_port       = scl_port;
    self->scl_pin        = scl_pin;
    self->sda_port       = sda_port;
    self->sda_pin        = sda_pin;
    self->half_period_us = (half_period_us == 0) ? 5U : half_period_us;
    self->mutex          = NULL;  /* created by hk_i2c_sw_rtos_init() */

    /* idle bus high */
    scl_release(self);
    sda_release(self);

    bus->ctx        = self;
    bus->write      = op_write;
    bus->read       = op_read;
    bus->write_read = op_write_read;
    bus->probe      = op_probe;
    bus->recover    = NULL;
    return HK_OK;
}


/* See hk_i2c_hw_rtos_init(): pre-scheduler FreeRTOS calls leave interrupts
 * masked, so the mutex is created separately, just before the scheduler. */
hk_status_t hk_i2c_sw_rtos_init(hk_i2c_sw_t *self)
{
    if (self == NULL) { return HK_ERR_PARAM; }
    self->mutex = xSemaphoreCreateMutex();
    return (self->mutex == NULL) ? HK_ERR : HK_OK;
}

#endif /* !HK_HOST */
