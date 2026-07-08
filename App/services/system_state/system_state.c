#include "services/system_state/system_state.h"

#include <string.h>

#if !defined(HK_HOST)
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t s_mtx;
#define STATE_LOCK()   (void)xSemaphoreTake(s_mtx, portMAX_DELAY)
#define STATE_UNLOCK() (void)xSemaphoreGive(s_mtx)
#else
#define STATE_LOCK()   ((void)0)
#define STATE_UNLOCK() ((void)0)
#endif

static hk_system_state_t s_state;

void hk_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mission = HK_MISSION_BOOT;
#if !defined(HK_HOST)
    s_mtx = xSemaphoreCreateMutex();
#endif
}

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
