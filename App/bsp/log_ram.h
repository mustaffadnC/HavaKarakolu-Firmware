#ifndef HK_LOG_RAM_H
#define HK_LOG_RAM_H

#include <stddef.h>
#include <stdint.h>

#if !defined(HK_HOST)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Log sink that keeps the most recent lines in a magic-tagged SRAM ring so a
 * debugger can read them out over SWD.
 *
 * The board has no debug UART and no telemetry link, and the SD log needs the
 * 5V rail, so until the power chain is fixed this is the only way to see what
 * the firmware is saying. Read it with CubeProgrammer:
 *
 *   STM32_Programmer_CLI -q -c port=SWD mode=HOTPLUG reset=HWrst \
 *       -r32 <address of g_hk_log_ram> <12 + HK_LOG_RAM_SIZE>
 *
 * (get the address from `arm-none-eabi-nm <elf> | grep g_hk_log_ram`; use
 * reset=HWrst, it gives a ~1.7 s window after boot instead of ~0.15 s).
 *
 * `wr` counts every byte ever handed to the sink and is never reset, so once
 * wr >= size the oldest surviving byte sits at buf[wr % size].
 */
#define HK_LOG_RAM_MAGIC 0x484B4C47u   /* 'H' 'K' 'L' 'G' */
#define HK_LOG_RAM_SIZE  2048u         /* power of two: keeps the wrap a mask */

typedef struct {
    uint32_t magic;   /* HK_LOG_RAM_MAGIC once initialised */
    uint32_t size;    /* HK_LOG_RAM_SIZE, so the reader need not assume */
    uint32_t wr;      /* total bytes written, monotonic, wraps naturally */
    uint8_t  buf[HK_LOG_RAM_SIZE];
} hk_log_ram_t;

extern hk_log_ram_t g_hk_log_ram;

/* Clear the ring and stamp the header. Call before hk_log_init(). */
void hk_log_ram_init(void);

/* hk_log_sink_fn compatible. Not ISR-safe (neither is the log facade). */
void hk_log_ram_sink(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* !HK_HOST */

#endif /* HK_LOG_RAM_H */
