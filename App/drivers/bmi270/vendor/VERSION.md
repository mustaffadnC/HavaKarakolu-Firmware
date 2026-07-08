# Bosch BMI270_SensorAPI — vendored copy

- **Source:** https://github.com/boschsensortec/BMI270_SensorAPI (tag v2.86.1)
- **Vendored:** 2026-07-08
- **License:** BSD-3-Clause, see LICENSE
- **Files:** bmi2.c/h, bmi2_defs.h, bmi270.c/h (base variant; the ~8 KB config
  blob lives in bmi270.c as `bmi270_config_file`)

Wrapper: `App/drivers/bmi270/bmi270_drv.c` (enabled with `HK_USE_BMI270`).
Do not edit these files; upgrade by re-vendoring a newer tag.
