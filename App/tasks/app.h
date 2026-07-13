#ifndef HK_APP_H
#define HK_APP_H

#if !defined(HK_HOST)

#include "main.h"

#include "bsp/board_config.h"   /* HK_HAS_BAT_SENSE, pin map */

#include "bus/i2c_hw.h"
#include "bus/i2c_sw.h"
#include "bus/spi_hw.h"
#include "bus/uart_dma.h"
#if HK_HAS_BAT_SENSE
#include "drivers/battery/battery.h"
#endif
#include "drivers/buzzer/buzzer.h"
#include "drivers/fan/fan.h"
#include "drivers/gps_ublox/gps_ublox.h"
#include "drivers/lock/lock.h"
#include "drivers/sd_spi/sd_spi.h"
#include "drivers/servo/servo.h"
#include "bus/nv_if.h"
#include "services/config/config.h"
#include "services/mission/mission.h"
#include "services/sensor_manager/sensor_manager.h"
#include "services/storage/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Top-level application context: owns every bus, device and actuator. */
typedef struct {
    /* buses */
    hk_i2c_hw_t   i2c1_hw, i2c2_hw;
    hk_i2c_sw_t   swi2c_hw;
    hk_i2c_bus_t  i2c1, i2c2, swi2c;
    hk_spi_hw_t   spi1_hw;
    hk_spi_bus_t  spi1;
    hk_uart_dma_t gps_uart_dma;
    hk_uart_t     gps_uart;

    /* sensors */
    hk_sensor_mgr_t sensors;
    hk_gps_t        gps;
#if HK_HAS_BAT_SENSE
    hk_battery_t    batt;
#endif

    /* storage (SD card logging) */
    hk_sd_t      sd;
    hk_storage_t storage;

    /* mission state machine */
    hk_mission_t mission;

    /* configuration (NV flash journal + SD CONFIG.INI overlay) */
    hk_nv_t          nv;
    hk_config_body_t cfg;

    /* actuators */
    hk_servo_t   servo;
    hk_buzzer_t  buzzer;
    hk_fan_t     fan1, fan2;
    hk_lock_t    lock;
} hk_app_t;

extern hk_app_t g_app;

/* Initialise clocks-helpers, state, buses, devices, actuators.
 * Call from main() AFTER HAL/CubeMX peripheral init, BEFORE the scheduler. */
void hk_app_init(void);

/* Create the FreeRTOS tasks. Call before osKernelStart()/vTaskStartScheduler(). */
void hk_app_start(void);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_APP_H */
