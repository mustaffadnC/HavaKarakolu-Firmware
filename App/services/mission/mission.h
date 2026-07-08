#ifndef HK_MISSION_H
#define HK_MISSION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mission phases (see plan §6). The state machine logic is implemented in
 * mission.c (F6-F7); this header defines the enum + name so system_state and
 * telemetry can reference it now.
 *
 *  BOOT -> SELFTEST -> ATTACHED -> ARMED -> RELEASE -> DESCENT -> LANDED -> RECOVERY
 *                          ^___________(disarm)__________|
 */
typedef enum {
    HK_MISSION_BOOT = 0,
    HK_MISSION_SELFTEST,
    HK_MISSION_ATTACHED,
    HK_MISSION_ARMED,
    HK_MISSION_RELEASE,
    HK_MISSION_DESCENT,
    HK_MISSION_LANDED,
    HK_MISSION_RECOVERY,
    HK_MISSION_COUNT
} hk_mission_state_t;

static inline const char *hk_mission_state_str(hk_mission_state_t s)
{
    switch (s) {
    case HK_MISSION_BOOT:     return "BOOT";
    case HK_MISSION_SELFTEST: return "SELFTEST";
    case HK_MISSION_ATTACHED: return "ATTACHED";
    case HK_MISSION_ARMED:    return "ARMED";
    case HK_MISSION_RELEASE:  return "RELEASE";
    case HK_MISSION_DESCENT:  return "DESCENT";
    case HK_MISSION_LANDED:   return "LANDED";
    case HK_MISSION_RECOVERY: return "RECOVERY";
    default:                  return "?";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HK_MISSION_H */
