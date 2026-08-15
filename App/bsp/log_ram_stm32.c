/*
 * SRAM ring-buffer log sink readable over SWD. See bsp/log_ram.h for the
 * layout and the CubeProgrammer command that dumps it.
 *
 * Deliberately free of FreeRTOS calls: hk_app_init() installs this sink before
 * the scheduler exists, and any FreeRTOS critical section taken that early
 * leaves interrupts masked forever, because uxCriticalNesting still holds its
 * 0xAAAAAAAA sentinel so the matching exit never reaches zero (docs/
 * DEVIR-TESLIM.md 9.1). Saving and restoring PRIMASK is correct both before
 * and after the scheduler starts, and nests safely inside a FreeRTOS critical
 * section.
 */
#if !defined(HK_HOST)

#include "bsp/log_ram.h"

#include "main.h"   /* CMSIS core intrinsics: __get_PRIMASK, __disable_irq */

hk_log_ram_t g_hk_log_ram;

void hk_log_ram_init(void)
{
    g_hk_log_ram.magic = 0u;          /* invalid while the ring is being cleared */
    g_hk_log_ram.wr    = 0u;
    g_hk_log_ram.size  = HK_LOG_RAM_SIZE;
    for (uint32_t i = 0u; i < HK_LOG_RAM_SIZE; ++i) {
        g_hk_log_ram.buf[i] = 0u;
    }
    g_hk_log_ram.magic = HK_LOG_RAM_MAGIC;   /* stamped last */
}

void hk_log_ram_sink(const char *data, size_t len)
{
    if (data == NULL || len == 0u) {
        return;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint32_t w = g_hk_log_ram.wr;
    for (size_t i = 0u; i < len; ++i) {
        g_hk_log_ram.buf[w & (HK_LOG_RAM_SIZE - 1u)] = (uint8_t)data[i];
        ++w;
    }
    g_hk_log_ram.wr = w;

    __set_PRIMASK(primask);
}

#endif /* !HK_HOST */
