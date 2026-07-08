#include "bus/i2c_scan.h"

#include "common/log.h"

int hk_i2c_scan(const hk_i2c_bus_t *bus, const char *bus_name)
{
    int found = 0;
    HK_LOGI("i2cscan", "scanning %s ...", (bus_name != NULL) ? bus_name : "?");
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        if (hk_i2c_probe(bus, addr) == HK_OK) {
            HK_LOGI("i2cscan", "  found 0x%02X", addr);
            ++found;
        }
    }
    HK_LOGI("i2cscan", "%s: %d device(s)", (bus_name != NULL) ? bus_name : "?", found);
    return found;
}
