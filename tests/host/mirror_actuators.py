#!/usr/bin/env python3
"""
Algorithm mirror of the F3 actuator math (servo/buzzer/fan/battery/ws2812).

This re-implements the SAME formulas as the C in App/drivers/* and checks the
SAME vectors as test_actuators.c. It exists because Smart App Control on this
machine blocks running freshly built unsigned native binaries; the C itself
compiles cleanly and runs identically where execution is allowed (CI/Linux).
This corroborates the constants/formulas, not the C compilation.
"""
import sys

fails = 0
def check(cond, msg):
    global fails
    if not cond:
        fails += 1
        print(f"  FAIL: {msg}")

# ---- servo: angle -> pulse us ----
def servo_us(deg, lo, hi):
    deg = max(0.0, min(deg, 180.0))
    return int(lo + (hi - lo) * (deg / 180.0) + 0.5)

check(servo_us(0, 1000, 2000) == 1000, "servo 0deg")
check(servo_us(180, 1000, 2000) == 2000, "servo 180deg")
check(servo_us(90, 1000, 2000) == 1500, "servo 90deg")
check(servo_us(-50, 1000, 2000) == 1000, "servo clamp")

# ---- buzzer: timer psc/arr ----
def buzzer_calc(clk, freq):
    if clk == 0 or freq == 0:
        return None
    period = clk // freq
    if period < 2:
        return None
    p = (period - 1) // 65536
    a = period // (p + 1)
    if a < 1:
        return None
    a -= 1
    if a > 0xFFFF or p > 0xFFFF:
        return None
    return p, a

r = buzzer_calc(84_000_000, 1000)
check(r == (1, 41999), f"buzzer 1kHz -> {r}")
check(84_000_000 // ((r[0] + 1) * (r[1] + 1)) == 1000, "buzzer freq back-calc")
check(buzzer_calc(84_000_000, 84_000_000) is None, "buzzer too-high freq")

# ---- fan hysteresis ----
def fan(t, on, off, cur):
    return (not (t <= off)) if cur else (t >= on)

check(fan(30, 35, 30, False) is False, "fan off stays off")
check(fan(36, 35, 30, False) is True,  "fan off -> on")
check(fan(31, 35, 30, True) is True,   "fan on stays on")
check(fan(30, 35, 30, True) is False,  "fan on -> off")

# ---- battery ----
def bat_v(raw, vref, fs, ratio):
    return raw / fs * vref * ratio
def bat_soc(v, cells):
    per = v / cells
    return max(0.0, min((per - 3.30) / 0.90, 1.0))

check(abs(bat_v(2048, 3.3, 4095.0, 5.54545) - 9.153) < 0.05, "battery voltage")
check(abs(bat_soc(11.1, 3) - 0.444) < 0.02, "soc nominal")
check(abs(bat_soc(12.6, 3) - 1.0) < 0.001, "soc full")
check(abs(bat_soc(9.9, 3) - 0.0) < 0.001, "soc empty")

# ---- ws2812 GRB encode ----
def ws_encode(r, g, b, hi, lo):
    out = []
    for byte in (g, r, b):           # GRB order
        for bit in range(7, -1, -1):  # MSB first
            out.append(hi if (byte >> bit) & 1 else lo)
    return out

buf = ws_encode(0x01, 0x80, 0x00, 58, 29)
check(buf[0] == 58,  "ws G bit7=1")
check(buf[1] == 29,  "ws G bit6=0")
check(buf[8] == 29,  "ws R bit7=0")
check(buf[15] == 58, "ws R bit0=1")
check(buf[16] == 29, "ws B bit7=0")
check(buf[23] == 29, "ws B bit0=0")

total = 4 + 3 + 4 + 4 + 6
print(f"mirror_actuators: {total} checks, {fails} failed")
sys.exit(1 if fails else 0)
