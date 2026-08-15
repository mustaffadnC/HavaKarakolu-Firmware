# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **Continuing on a fresh machine?** Read `docs/DEVIR-TESLIM.md` first — it has
> from-zero tool installation, how to rebuild the out-of-repo CubeMX project,
> flashing/memory-read commands, and the live hardware bring-up status.

## Project

Ülgen ÇARGE capsule firmware: STM32F407VGTx, FreeRTOS (CMSIS-RTOS v2), C11.
The capsule releases from a carrier (servo + solenoid), logs environment data
(2× SHT4x, BMP581, BMI270, MAX-M10S GPS) to an SD card — the board's ONLY data
output — and runs thermal control via two fans. Docs are in Turkish; code and
commit messages in English.

## Hard constraints (read first)

- **Windows Smart App Control blocks running locally built executables.**
  Local builds are compile-checks only; tests actually RUN only in GitHub
  Actions (Ubuntu). A phase is "done" when CI is green, not when it builds.
- **No AI traces in commits.** Never add `Co-Authored-By: Claude` or
  "Generated with" trailers — this overrides the default commit format.
  Style: Conventional Commits (`feat(bmp581): ...`).
- **`gh` CLI is installed but its token lacks scopes.** Talk to the GitHub API
  with curl using the git-credential-manager token:
  `TOKEN=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill | grep '^password=' | cut -d= -f2)`
  Repo: `mustaffadnC/HavaKarakolu-Firmware` (private; moved from `conny0506`,
  the old URL still redirects).
- CI sometimes fails in "Set up job" with `Service Unavailable` — that is
  GitHub infrastructure, not the code. Re-run (`.../actions/runs/<id>/rerun-failed-jobs`)
  or trigger `workflow_dispatch` on `host-tests.yml`.

## Commands

```bash
# Host tests — configure + build locally (Git Bash; gcc/cmake from WinLibs winget)
cd tests/host && cmake -B build -S . -G "MinGW Makefiles" && cmake --build build

# Run tests: ONLY in CI (push to main triggers .github/workflows/host-tests.yml).
# On Linux/CI a single test is: ctest --test-dir tests/host/build -R test_mission

# Log reader spec self-check (also a CI step)
python3 tools/hk_log_reader.py --selftest        # decode a real log: tools/hk_log_reader.py FL_0001.BIN

# Target firmware (ARM .elf/.bin) — run from the CubeMX project dir, NOT the repo:
cd /c/Users/pc/STM32CubeIDE/hk-capsule-fw && bash build_hk.sh
# (script reference copy: tools/target-build/build_hk.sh; uses CubeIDE's arm-none-eabi-gcc)

# Regenerate Core/Drivers/Middlewares after editing hk-capsule-fw.ioc (headless CubeMX).
# Use forward slashes: bash printf reads the \U of C:\Users as a unicode escape and dies.
MX="$LOCALAPPDATA/Programs/STM32CubeMX"
cat > gen.txt <<'EOF'
config load C:/Users/pc/STM32CubeIDE/hk-capsule-fw/hk-capsule-fw.ioc
project generate
exit
EOF
"$MX/jre/bin/java.exe" -jar "$MX/STM32CubeMX.exe" -q gen.txt
```

Adding a host test: create `tests/host/test_<x>.c` with its own `main()` ending
in `return hk_test_summary();` (harness: `tests/host/hk_test.h`), register the
executable AND add it to the `foreach(...)` list in `tests/host/CMakeLists.txt`.

## Architecture

**Two-layer split.** This repo holds only the portable `App/` layer + host
tests. The CubeMX-generated `Core/`, `Drivers/` (HAL), `Middlewares/`
(FreeRTOS) live OUTSIDE the repo in `C:\Users\pc\STM32CubeIDE\hk-capsule-fw\`
(which has an `App` junction back into this repo). The repo's
`hk-capsule-fw.ioc` is the archived source of that project;
`docs/cubemx-setup.md` documents the whole integration (linker FLASH must be
capped at 896K — sector 11 is the config NV area).

**Dependency injection is the load-bearing pattern.** Drivers never call HAL
directly; they depend on the interfaces in `App/bus/` (`i2c_bus_if`, `spi_if`,
`uart_if`, `nv_if`). Target implementations (`i2c_hw`, `spi_hw`, `uart_dma`,
`bsp/nv_flash_stm32`) are compiled only on target; host tests compile the same
drivers against mocks in `tests/host/mocks/` (scripted I2C, behavioral SD-card
model, RAM disk with power-loss snapshot, RAM-backed NV). `HK_HOST=1` guards
every target-only file — host tests must never include HAL or
`bsp/board_config.h`.

**One board, no build variants.** `App/bsp/board_config.h` is the single
source of truth and was checked line by line against the schematic on
2026-07-29: buzzer PB5/TIM3_CH2, FAN1 PB12, FAN2 PB15, solenoid PB0,
BAT_TEST PC0/ADC1_IN10, BMI270 INT1 PC4, SWD correctly on PA13/PA14, PB13
and PB14 unconnected. The schematic symbol says STM32F405VGTx but an
STM32F407VGT6 is fitted (pin- and register-compatible here).
Earlier revisions had `HK_BOARD_SUKRU` / `HK_BOARD_REV2A` switches — the
second PCB was dropped from the design, so both are gone. Do not reintroduce
a variant switch without a second physical board.

**Runtime data flow.** FreeRTOS tasks in `App/tasks/app.c`: sensor tasks (imu
100 Hz, env 2 Hz, gps) publish into the mutex-protected snapshot
`services/system_state`; `task_mission` (50 ms) feeds that into the PURE state
machine `services/mission/mission.c` (BOOT→…→RECOVERY, all thresholds in
`hk_mission_cfg_t`, every transition debounced, works baro-only/IMU-only) and
applies lock/servo/buzzer-pattern outputs; every task also pushes records to
`services/storage` (producer/consumer split: pushes serialize into a ring
queue under a critical-section lock, ONLY `task_storage` touches FatFs).
Log format: `/LOGS/FL_NNNN.BIN` (framed `'HK'|ver|type|len|payload|crc16`,
reader resyncs past corruption) + `.CSV` quick-look; `f_sync` every 1 s and
after every EVENT. A missing SD card = degraded mode with retries, never a
fault, and the card is NEVER auto-formatted.

**Config path.** `services/config`: append-journal records in flash sector 11
(newest CRC32-valid wins, torn writes fall back) overlaid by `/CONFIG.INI`
from the SD card — both applied in `hk_app_init()` BEFORE tasks exist, so
values are immutable at runtime. Field tuning (mission thresholds, battery
divider) needs no reflash.

**Solenoid semantics (safety-relevant).** Normally closed: de-energized =
LOCKED (EE answer S3). `hk_mission_out_t.lock_engaged == true` means
de-energized/at rest; the coil (~0.41 A) is energized only during the RELEASE
actuation window.

**Vendored code — never edit, upgrade by re-vendoring:**
`App/third_party/fatfs/` (R0.15a, config deltas in its VERSION.md) and
`App/drivers/bmi270/vendor/` (Bosch SensorAPI v2.86.1, enabled by
`HK_USE_BMI270`; without it the wrapper compiles as a stub). Both are compiled
with `-w`; everything else must stay warning-clean under
`-Wall -Wextra -Wshadow -Wconversion` (host) and `-Wall` (target).

## Key references

- `docs/bringup.md` — PCB arrival procedure (P9), equipment list, SWD-broken
  recovery flashing.
- `docs/ee-questions.md` — hardware Q&A with the electronics team (answered);
  the two-board situation and all confirmed electrical facts.
- `docs/cubemx-setup.md` — CubeMX/CubeIDE setup, headless generate + build.
- `README.md` — phase table and current status.
