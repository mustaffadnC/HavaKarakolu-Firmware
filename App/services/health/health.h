#ifndef HK_HEALTH_H
#define HK_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-task liveness flags. Each monitored task kicks its bit every cycle;
 * the health task only refreshes the IWDG when all expected tasks are alive. */
typedef enum {
    HK_TASK_IMU     = 1u << 0,
    HK_TASK_ENV     = 1u << 1,
    HK_TASK_GPS     = 1u << 2,
    HK_TASK_CONTROL = 1u << 3,
    HK_TASK_MISSION = 1u << 4,
    HK_TASK_TELEM   = 1u << 5
} hk_task_flag_t;

/* Pure/host-testable: are all expected task bits present in `kicked`? */
bool hk_health_all_alive(uint32_t kicked, uint32_t expected);

#if !defined(HK_HOST)

#include "main.h"

/* `expected` = OR of the hk_task_flag_t bits that must stay alive. */
void        hk_health_init(IWDG_HandleTypeDef *hiwdg, uint32_t expected);

/* Called by each monitored task once per loop. */
void        hk_health_kick(hk_task_flag_t task);

/* Called by the health task each period: refreshes the IWDG iff all expected
 * tasks have kicked since the last service, then clears the accumulator.
 * Returns the set of MISSING task bits (0 == all alive, IWDG refreshed). */
uint32_t    hk_health_service(void);

/* Human-readable reset cause from RCC flags (clears the flags). */
const char *hk_health_reset_reason(void);

#endif /* !HK_HOST */

#ifdef __cplusplus
}
#endif

#endif /* HK_HEALTH_H */
