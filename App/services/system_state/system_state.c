#include "services/system_state/system_state.h"

#include <string.h>

#if !defined(HK_HOST)
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t s_mtx;   /* NULL until hk_state_rtos_init() */
/* NULL-safe on purpose: hk_app_init() publishes sensor flags from here while
 * the scheduler is still down and the mutex deliberately does not exist. */
#define STATE_LOCK()                                          \
    do { if (s_mtx != NULL) {                                 \
        (void)xSemaphoreTake(s_mtx, portMAX_DELAY); }         \
    } while (0)
#define STATE_UNLOCK()                                        \
    do { if (s_mtx != NULL) {                                 \
        (void)xSemaphoreGive(s_mtx); }                        \
    } while (0)
#else
#define STATE_LOCK()   ((void)0)
#define STATE_UNLOCK() ((void)0)
#endif

static hk_system_state_t s_state;

void hk_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mission = HK_MISSION_BOOT;
}

#if !defined(HK_HOST)
void hk_state_rtos_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
}
#endif

void hk_state_get(hk_system_state_t *out)
{
    STATE_LOCK();
    *out = s_state;
    STATE_UNLOCK();
}

hk_system_state_t *hk_state_lock(void)
{
    STATE_LOCK();
    return &s_state;
}

void hk_state_unlock(void)
{
    STATE_UNLOCK();
}

void hk_state_set_sensor_ok(hk_sensor_flag_t flag, bool ok)
{
    STATE_LOCK();
    if (ok) {
        s_state.sensor_ok |= (uint32_t)flag;
    } else {
        s_state.sensor_ok &= ~(uint32_t)flag;
    }
    STATE_UNLOCK();
}
