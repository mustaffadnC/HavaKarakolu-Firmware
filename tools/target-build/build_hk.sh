#!/usr/bin/env bash
# Headless ARM target build for hk-capsule-fw (STM32F407VGTx), driven from bash
# so it does not depend on ST make's shell resolution. Toolchain = the
# arm-none-eabi-gcc bundled with STM32CubeIDE 2.2.0.
#
# Run from the CubeMX-GENERATED project directory (the one holding Core/,
# Drivers/, Middlewares/, *.ld and an "App" junction to this repo's App/):
#     bash build_hk.sh
# Produces build_hk/hk-capsule-fw.elf (+ .bin). Default board variant is
# HK_BOARD_SUKRU; add -DHK_BOARD_REV2A to DEFS for the first board.
#
# This is a reference copy kept in the repo; the live copy sits next to the
# generated project. See docs/cubemx-setup.md.
set -u
cd "$(dirname "$0")"

TOOLS="/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
CC="$TOOLS/arm-none-eabi-gcc.exe"
SZ="$TOOLS/arm-none-eabi-size.exe"
OBJCOPY="$TOOLS/arm-none-eabi-objcopy.exe"
BUILD=build_hk
TARGET=hk-capsule-fw

CPU="-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb"
DEFS="-DUSE_HAL_DRIVER -DSTM32F407xx -DHK_USE_BMI270"
OPT="-Og -g3"
INC="-ICore/Inc -IDrivers/STM32F4xx_HAL_Driver/Inc -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F4xx/Include -IDrivers/CMSIS/Include \
-IMiddlewares/Third_Party/FreeRTOS/Source/include \
-IMiddlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
-IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
-IApp -IApp/third_party/fatfs -IApp/drivers/bmi270/vendor"
CFLAGS="$CPU $DEFS $OPT $INC -ffunction-sections -fdata-sections"
WARN="-Wall"

RTOS="Middlewares/Third_Party/FreeRTOS/Source"
mapfile -t C_CORE < <(ls Core/Src/*.c)
mapfile -t C_HAL  < <(ls Drivers/STM32F4xx_HAL_Driver/Src/*.c | grep -v _template)

C_RTOS=(
  "$RTOS/croutine.c" "$RTOS/event_groups.c" "$RTOS/list.c" "$RTOS/queue.c"
  "$RTOS/stream_buffer.c" "$RTOS/tasks.c" "$RTOS/timers.c"
  "$RTOS/portable/GCC/ARM_CM4F/port.c" "$RTOS/portable/MemMang/heap_4.c"
  "$RTOS/CMSIS_RTOS_V2/cmsis_os2.c"
)
C_APP=(
  App/bsp/*.c App/bus/*.c App/common/*.c
  App/drivers/bmp280/bmp280.c App/drivers/sht4x/sht4x.c
  App/drivers/gps_ublox/gps_ublox.c App/drivers/gps_ublox/nmea.c
  App/drivers/bmi270/bmi270_drv.c App/drivers/sd_spi/sd_spi.c
  App/drivers/servo/servo.c App/drivers/buzzer/buzzer.c
  App/drivers/fan/fan.c App/drivers/lock/lock.c App/drivers/battery/battery.c
  App/services/sensor_manager/*.c App/services/system_state/*.c
  App/services/health/*.c App/services/mission/mission.c
  App/services/storage/storage.c App/services/storage/diskio_sd.c
  App/services/config/*.c App/tasks/*.c
)
C_VENDOR=(
  App/third_party/fatfs/ff.c App/third_party/fatfs/ffunicode.c
  App/drivers/bmi270/vendor/bmi2.c App/drivers/bmi270/vendor/bmi270.c
)

OBJS=()
FAIL=0

compile() {  # $1=src  $2=extra flags
  local src="$1" flags="$2"
  local obj="$BUILD/${src%.c}.o"
  mkdir -p "$(dirname "$obj")"
  if ! "$CC" -c $CFLAGS $flags "$src" -o "$obj" 2> "$obj.err"; then
    echo "### COMPILE FAILED: $src"
    cat "$obj.err"
    FAIL=1
    return
  fi
  [ -s "$obj.err" ] && { echo "--- warnings in $src:"; cat "$obj.err"; }
  OBJS+=("$obj")
}

echo "== compiling Core/HAL/RTOS/App ($(( ${#C_CORE[@]} + ${#C_HAL[@]} + ${#C_RTOS[@]} )) sys + App) =="
for f in "${C_CORE[@]}" "${C_HAL[@]}" "${C_RTOS[@]}" "${C_APP[@]}"; do compile "$f" "$WARN"; done
echo "== compiling vendored (FatFs, BMI270) with -w =="
for f in "${C_VENDOR[@]}"; do compile "$f" "-w"; done

# startup
mkdir -p "$BUILD"
"$CC" -c $CPU -x assembler-with-cpp Core/Startup/startup_stm32f407vgtx.s -o "$BUILD/startup.o" || FAIL=1
OBJS+=("$BUILD/startup.o")

if [ "$FAIL" -ne 0 ]; then echo "=== BUILD ABORTED (compile errors above) ==="; exit 1; fi

echo "== linking =="
LDFLAGS="$CPU -TSTM32F407VGTX_FLASH.ld -specs=nano.specs -specs=nosys.specs \
-Wl,-Map=$BUILD/$TARGET.map,--cref -Wl,--gc-sections -Wl,--print-memory-usage -u_printf_float -lc -lm"
if ! "$CC" "${OBJS[@]}" $LDFLAGS -o "$BUILD/$TARGET.elf"; then
  echo "=== LINK FAILED ==="; exit 1
fi
"$OBJCOPY" -O binary "$BUILD/$TARGET.elf" "$BUILD/$TARGET.bin"
echo "== size =="
"$SZ" "$BUILD/$TARGET.elf"
echo "=== BUILD OK: $BUILD/$TARGET.elf ==="
