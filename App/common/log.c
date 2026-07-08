#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static hk_log_sink_fn s_sink   = NULL;
static hk_log_level_t s_max    = HK_LOG_INFO;

static const char *level_tag(hk_log_level_t level)
{
    switch (level) {
    case HK_LOG_ERROR: return "E";
    case HK_LOG_WARN:  return "W";
    case HK_LOG_INFO:  return "I";
    case HK_LOG_DEBUG: return "D";
    default:           return "?";
    }
}

void hk_log_init(hk_log_sink_fn sink, hk_log_level_t max_level)
{
    s_sink = sink;
    s_max  = max_level;
}

void hk_log_set_level(hk_log_level_t max_level)
{
    s_max = max_level;
}

void hk_log_write(hk_log_level_t level, const char *tag, const char *fmt, ...)
{
    if (s_sink == NULL || level > s_max) {
        return;
    }

    char    line[160];
    int     n;

    n = snprintf(line, sizeof(line), "[%s/%s] ", level_tag(level),
                 (tag != NULL) ? tag : "-");
    if (n < 0) {
        return;
    }
    if ((size_t)n >= sizeof(line)) {
        n = (int)sizeof(line) - 1;
    }

    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
    va_end(ap);

    if (m < 0) {
        return;
    }

    size_t total = (size_t)n + (size_t)m;
    if (total >= sizeof(line)) {
        total = sizeof(line) - 1;
    }
    /* Append newline if room. */
    if (total < sizeof(line) - 1) {
        line[total++] = '\n';
    }

    s_sink(line, total);
}
