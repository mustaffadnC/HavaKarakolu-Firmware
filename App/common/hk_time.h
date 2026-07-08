#ifndef HK_TIME_H
#define HK_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable time hooks. Implemented per platform:
 *   - bsp/hk_time_stm32.c : HAL_GetTick + DWT (target)
 *   - tests provide their own shims (host)
 *
 * hk_delay_* are blocking; on the target after the scheduler starts, prefer
 * RTOS delays in tasks — these remain for short driver-level waits.
 */
void     hk_time_init(void);
uint32_t hk_millis(void);
void     hk_delay_ms(uint32_t ms);
void     hk_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* HK_TIME_H */
