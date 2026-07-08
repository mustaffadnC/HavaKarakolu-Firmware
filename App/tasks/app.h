#ifndef HK_APP_H
#define HK_APP_H

#if !defined(HK_HOST)

#include "main.h"

#include "bus/i2c_hw.h"
#include "bus/i2c_sw.h"
#include "bus/uart_dma.h"
#include "drivers/battery/battery.h"
#include "drivers/buzzer/buzzer.h"
#include "drivers/fan/fan.h"
#include "drivers/gps_ublox/gps_ublox.h"
#include "drivers/lock/lock.h"
#include "drivers/servo/servo.h"
#include "services/sensor_manager/sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Top-level application context: owns every bus, device and actuator. */
typedef struct {
    /* buses */
    hk_i2c_hw_t   i2c1_hw, i2c2_hw;
    hk_i2c_sw_t   swi2c_hw;
    hk_i2c_bus_t  i2c1, i2c2, swi2c;
    hk_uart_dma_t gps_uart_dma;
    hk_uart_t     gps_uart;

    /* sensors */
    hk_sensor_mgr_t sensors;
    hk_gps_t        gps;
    hk_battery_t    batt;

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
