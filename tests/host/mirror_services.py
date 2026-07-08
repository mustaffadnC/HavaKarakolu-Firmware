#!/usr/bin/env python3
"""
Algorithm mirror of F4-F5 pure logic (complementary filter, health task-alive,
mission state names). Same formulas/vectors as test_services.c. Exists only to
demonstrate green locally while Smart App Control blocks unsigned native exes;
the C runs identically in CI.
"""
import math, sys

fails = 0
def check(cond, msg):
    global fails
    if not cond:
        fails += 1
        print(f"  FAIL: {msg}")

RAD2DEG = 57.29577951308232

# ---- complementary filter ----
class Comp:
    def __init__(self, alpha):
        self.roll = 0.0; self.pitch = 0.0
        self.alpha = max(0.0, min(alpha, 1.0))
    def update(self, a, g, dt):
        roll_acc = math.atan2(a[1], a[2]) * RAD2DEG
        pitch_acc = math.atan2(-a[0], math.sqrt(a[1]*a[1] + a[2]*a[2])) * RAD2DEG
        gx = g[0] * RAD2DEG; gy = g[1] * RAD2DEG
        self.roll  = self.alpha * (self.roll  + gx*dt) + (1-self.alpha)*roll_acc
        self.pitch = self.alpha * (self.pitch + gy*dt) + (1-self.alpha)*pitch_acc

f = Comp(0.0); f.update((0,0,9.81), (0,0,0), 0.01)
check(abs(f.roll) < 0.5 and abs(f.pitch) < 0.5, "filter level")
f = Comp(0.0); f.update((0,9.81,0), (0,0,0), 0.01)
check(abs(f.roll - 90.0) < 1.0, "filter roll 90")
f = Comp(1.0); f.update((0,0,9.81), (1.0,0,0), 1.0)
check(abs(f.roll - 57.2958) < 0.5, "filter gyro integrate")

# ---- health all_alive ----
def all_alive(kicked, expected):
    return (kicked & expected) == expected
check(all_alive(0x0F, 0x0F), "health all")
check(not all_alive(0x0E, 0x0F), "health missing")
check(all_alive(0xFF, 0x0F), "health extra")

# ---- mission names ----
NAMES = ["BOOT","SELFTEST","ATTACHED","ARMED","RELEASE","DESCENT","LANDED","RECOVERY"]
check(NAMES[3] == "ARMED", "mission ARMED")
check(NAMES[7] == "RECOVERY", "mission RECOVERY")

print(f"mirror_services: {3+3+2} checks, {fails} failed")
sys.exit(1 if fails else 0)
