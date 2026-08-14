#ifndef HK_UART_DMA_H
#define HK_UART_DMA_H

#include "bus/uart_if.h"
#include "common/ringbuf.h"

#if !defined(HK_HOST)

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32 HAL UART with circular DMA RX + IDLE-line events feeding an SPSC ring
 * buffer; TX is mutex-guarded blocking (upgradeable to IT/DMA later).
 *
 * CubeMX requirements:
 *   - UART RX DMA channel set to CIRCULAR mode
 *   - UART global interrupt enabled
 * Call HAL_UARTEx_ReceiveToIdle_DMA is issued internally by hk_uart_dma_init().
 * The shared HAL_UARTEx_RxEventCallback dispatcher is provided in uart_dma.c.
 */
typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t            *dma_buf;
    size_t              dma_cap;
    size_t              last_pos;
    hk_ringbuf_t        rx;
    void               *tx_mutex;   /* SemaphoreHandle_t */
    uint32_t            tx_timeout_ms;
} hk_uart_dma_t;

/* dma_storage: DMA landing buffer (RAM, not CCM). rx_storage: ring buffer
 * backing, capacity MUST be a power of two. */
hk_status_t hk_uart_dma_init(hk_uart_dma_t *self, hk_uart_t *uart,
                             UART_HandleTypeDef *huart,
                             uint8_t *dma_storage, size_t dma_cap,
                             uint8_t *rx_storage, size_t rx_cap,
                             uint32_t tx_timeout_ms);

/* Call from HAL_UARTEx_RxEventCallback (dispatcher does this automatically). */
void hk_uart_dma_on_rx_event(hk_uart_dma_t *self, size_t pos);


hk_status_t hk_uart_dma_rtos_init(hk_uart_dma_t *self);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */
#endif /* HK_UART_DMA_H */
