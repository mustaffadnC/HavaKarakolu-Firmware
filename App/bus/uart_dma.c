#include "bus/uart_dma.h"

#if !defined(HK_HOST)

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

/* ---- instance registry for the shared RxEvent callback ---- */
#define HK_UART_MAX_INSTANCES 4
static hk_uart_dma_t *s_instances[HK_UART_MAX_INSTANCES];

static void register_instance(hk_uart_dma_t *self)
{
    for (int i = 0; i < HK_UART_MAX_INSTANCES; ++i) {
        if (s_instances[i] == NULL) {
            s_instances[i] = self;
            return;
        }
    }
}

/* ---- RX ingest (called from ISR context via RxEventCallback) ---- */
void hk_uart_dma_on_rx_event(hk_uart_dma_t *self, size_t pos)
{
    if (pos > self->dma_cap) {
        pos = self->dma_cap;
    }
    if (pos == self->last_pos) {
        return;
    }
    if (pos > self->last_pos) {
        (void)hk_ringbuf_write(&self->rx, &self->dma_buf[self->last_pos],
                               pos - self->last_pos);
    } else {
        /* wrapped */
        (void)hk_ringbuf_write(&self->rx, &self->dma_buf[self->last_pos],
                               self->dma_cap - self->last_pos);
        (void)hk_ringbuf_write(&self->rx, &self->dma_buf[0], pos);
    }
    self->last_pos = (pos == self->dma_cap) ? 0U : pos;
}

/* ---- uart_if implementation ---- */

static int op_write(void *ctx, const uint8_t *data, size_t len)
{
    hk_uart_dma_t *s = (hk_uart_dma_t *)ctx;
    if (s->tx_mutex != NULL &&
        xSemaphoreTake((SemaphoreHandle_t)s->tx_mutex,
                       pdMS_TO_TICKS(s->tx_timeout_ms)) != pdTRUE) {
        return HK_ERR_BUSY;
    }
    HAL_StatusTypeDef hs = HAL_UART_Transmit(s->huart, (uint8_t *)data,
                                             (uint16_t)len, s->tx_timeout_ms);
    if (s->tx_mutex != NULL) {
        (void)xSemaphoreGive((SemaphoreHandle_t)s->tx_mutex);
    }
    if (hs != HAL_OK) {
        return (hs == HAL_TIMEOUT) ? HK_ERR_TIMEOUT : HK_ERR_IO;
    }
    return (int)len;
}

static size_t op_read(void *ctx, uint8_t *out, size_t len)
{
    hk_uart_dma_t *s = (hk_uart_dma_t *)ctx;
    return hk_ringbuf_read(&s->rx, out, len);
}

static size_t op_available(void *ctx)
{
    hk_uart_dma_t *s = (hk_uart_dma_t *)ctx;
    return hk_ringbuf_count(&s->rx);
}

hk_status_t hk_uart_dma_init(hk_uart_dma_t *self, hk_uart_t *uart,
                             UART_HandleTypeDef *huart,
                             uint8_t *dma_storage, size_t dma_cap,
                             uint8_t *rx_storage, size_t rx_cap,
                             uint32_t tx_timeout_ms)
{
    if (self == NULL || uart == NULL || huart == NULL ||
        dma_storage == NULL || rx_storage == NULL) {
        return HK_ERR_PARAM;
    }
    if (!hk_ringbuf_init(&self->rx, rx_storage, rx_cap)) {
        return HK_ERR_PARAM;  /* rx_cap not power of two */
    }

    self->huart         = huart;
    self->dma_buf       = dma_storage;
    self->dma_cap       = dma_cap;
    self->last_pos      = 0U;
    self->tx_timeout_ms = (tx_timeout_ms == 0) ? 100U : tx_timeout_ms;
    self->tx_mutex      = NULL; /* created by hk_uart_dma_rtos_init() */

    register_instance(self);

    uart->ctx       = self;
    uart->write     = op_write;
    uart->read      = op_read;
    uart->available = op_available;

    if (HAL_UARTEx_ReceiveToIdle_DMA(huart, dma_storage, (uint16_t)dma_cap) != HAL_OK) {
        return HK_ERR_IO;
    }
    /* Disable the half-transfer interrupt noise if desired; events still fire
       on IDLE and TC, which is enough for streaming parsers. */
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
    return HK_OK;
}

/* ---- shared HAL callback: dispatch to the matching instance ----
 * If the application already defines this elsewhere, remove this one. */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    for (int i = 0; i < HK_UART_MAX_INSTANCES; ++i) {
        if (s_instances[i] != NULL && s_instances[i]->huart == huart) {
            hk_uart_dma_on_rx_event(s_instances[i], size);
            return;
        }
    }
}


/* See hk_i2c_hw_rtos_init(): created just before the scheduler starts. */
hk_status_t hk_uart_dma_rtos_init(hk_uart_dma_t *self)
{
    if (self == NULL) { return HK_ERR_PARAM; }
    self->tx_mutex = xSemaphoreCreateMutex();
    return (self->tx_mutex == NULL) ? HK_ERR : HK_OK;
}

#endif /* !HK_HOST */
