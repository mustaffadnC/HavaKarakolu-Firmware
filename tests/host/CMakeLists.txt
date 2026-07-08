cmake_minimum_required(VERSION 3.16)
project(hava_karakolu_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Host build: drivers depend on bus interfaces, never on the HAL.
add_compile_definitions(HK_HOST=1)
add_compile_options(-Wall -Wextra -Wshadow -Wconversion -O1 -g)

# App/ is the include root so headers resolve as "common/crc.h" etc.
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../App)
include_directories(${APP_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()

# Each test is a standalone executable; add the App sources it exercises.
add_executable(test_crc     test_crc.c     ${APP_DIR}/common/crc.c)
add_executable(test_ringbuf test_ringbuf.c ${APP_DIR}/common/ringbuf.c)
add_executable(test_sht4x   test_sht4x.c   ${APP_DIR}/drivers/sht4x/sht4x.c
                                           ${APP_DIR}/common/crc.c)
add_executable(test_gps     test_gps.c     ${APP_DIR}/drivers/gps_ublox/nmea.c)
add_executable(test_actuators test_actuators.c
                                           ${APP_DIR}/drivers/servo/servo.c
                                           ${APP_DIR}/drivers/buzzer/buzzer.c
                                           ${APP_DIR}/drivers/fan/fan.c
                                           ${APP_DIR}/drivers/battery/battery.c
                                           ${APP_DIR}/drivers/ws2812/ws2812.c)
add_executable(test_services test_services.c
                                           ${APP_DIR}/common/filters.c
                                           ${APP_DIR}/services/health/health.c)
add_executable(test_bmp280  test_bmp280.c  mocks/mock_i2c.c
                                           ${APP_DIR}/drivers/bmp280/bmp280.c)

# Math (powf) needs libm on some toolchains.
foreach(t test_crc test_ringbuf test_sht4x test_gps test_actuators test_services
          test_bmp280)
    target_link_libraries(${t} m)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# Future tests (telemetry, state machine) are appended here as the
# corresponding modules land in later phases.
