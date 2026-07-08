#ifndef HK_LOG_H
#define HK_LOG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lightweight logging facade. The backend is a byte-sink function pointer
 * (SEGGER RTT, a debug UART, or a host stdout shim). NOT ISR-safe: call from
 * tasks only. Output is formatted into a small static buffer.
 */
typedef enum {
    HK_LOG_ERROR = 0,
    HK_LOG_WARN  = 1,
    HK_LOG_INFO  = 2,
    HK_LOG_DEBUG = 3
} hk_log_level_t;

typedef void (*hk_log_sink_fn)(const char *data, size_t len);

void hk_log_init(hk_log_sink_fn sink, hk_log_level_t max_level);
void hk_log_set_level(hk_log_level_t max_level);

/* printf-style; prefixes with a level tag. */
void hk_log_write(hk_log_level_t level, const char *tag, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

#define HK_LOGE(tag, ...) hk_log_write(HK_LOG_ERROR, (tag), __VA_ARGS__)
#define HK_LOGW(tag, ...) hk_log_write(HK_LOG_WARN,  (tag), __VA_ARGS__)
#define HK_LOGI(tag, ...) hk_log_write(HK_LOG_INFO,  (tag), __VA_ARGS__)
#define HK_LOGD(tag, ...) hk_log_write(HK_LOG_DEBUG, (tag), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* HK_LOG_H */
