#ifndef HK_I2C_SCAN_H
#define HK_I2C_SCAN_H

#include <stdint.h>

#include "bus/i2c_bus_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Probe the standard 7-bit address range [0x08, 0x77] and report ACKing
 * devices via the log facility (tag "i2cscan"). Returns the count found.
 * `bus_name` is printed for context (e.g. "I2C1").
 */
int hk_i2c_scan(const hk_i2c_bus_t *bus, const char *bus_name);

#ifdef __cplusplus
}
#endif

#endif /* HK_I2C_SCAN_H */
