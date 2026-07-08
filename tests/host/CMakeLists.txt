cmake_minimum_required(VERSION 3.16)
project(hava_karakolu_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Host build: drivers depend on bus interfaces, never on the HAL.
add_compile_definitions(HK_HOST=1)
add_compile_options(-Wall -Wextra -Wshadow -Wconversion -O1 -g)

# App/ is the include root so headers resolve as "common/crc.h" etc.
# third_party/fatfs provides "ff.h"/"diskio.h" for the storage stack.
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../App)
include_directories(${APP_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                    ${APP_DIR}/third_party/fatfs)

# Vendored code is not held to our warning level.
set_source_files_properties(${APP_DIR}/third_party/fatfs/ff.c
                            ${APP_DIR}/drivers/bmi270/vendor/bmi2.c
                            ${APP_DIR}/drivers/bmi270/vendor/bmi270.c
                            PROPERTIES COMPILE_OPTIONS "-w")

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
add_executable(test_sd_spi  test_sd_spi.c  mocks/mock_spi.c
                                           ${APP_DIR}/drivers/sd_spi/sd_spi.c
                                           ${APP_DIR}/common/crc.c)
add_executable(test_storage test_storage.c mocks/diskio_ram.c
                                           ${APP_DIR}/services/storage/storage.c
                                           ${APP_DIR}/common/crc.c
                                           ${APP_DIR}/common/ringbuf.c
                                           ${APP_DIR}/third_party/fatfs/ff.c)
add_executable(test_bmi270  test_bmi270.c
                                           ${APP_DIR}/drivers/bmi270/bmi270_drv.c
                                           ${APP_DIR}/drivers/bmi270/vendor/bmi2.c
                                           ${APP_DIR}/drivers/bmi270/vendor/bmi270.c)
target_compile_definitions(test_bmi270 PRIVATE HK_USE_BMI270=1)
target_include_directories(test_bmi270 PRIVATE ${APP_DIR}/drivers/bmi270/vendor)
add_executable(test_mission test_mission.c
                                           ${APP_DIR}/services/mission/mission.c
                                           ${APP_DIR}/common/filters.c)
add_executable(test_flight_e2e test_flight_e2e.c
                                           sim/flight_sim.c
                                           mocks/diskio_ram.c
                                           ${APP_DIR}/services/mission/mission.c
                                           ${APP_DIR}/services/storage/storage.c
                                           ${APP_DIR}/drivers/gps_ublox/nmea.c
                                           ${APP_DIR}/common/filters.c
                                           ${APP_DIR}/common/crc.c
                                           ${APP_DIR}/common/ringbuf.c
                                           ${APP_DIR}/third_party/fatfs/ff.c)

# Math (powf) needs libm on some toolchains.
foreach(t test_crc test_ringbuf test_sht4x test_gps test_actuators test_services
          test_bmp280 test_sd_spi test_storage test_bmi270 test_mission
          test_flight_e2e)
    target_link_libraries(${t} m)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# Future tests (telemetry, state machine) are appended here as the
# corresponding modules land in later phases.
