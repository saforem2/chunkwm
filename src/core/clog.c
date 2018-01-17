#include "clog.h"

#include <stdio.h>
#include <stdarg.h>

enum c_log_level c_log_active_level = C_LOG_LEVEL_ERROR;

static void
c_log_debug(const char *format, va_list args)
{
    vfprintf(stdout, format, args);
}

static void
c_log_warn(const char *format, va_list args)
{
    vfprintf(stderr, format, args);
}

static void
c_log_error(const char *format, va_list args)
{
    vfprintf(stderr, format, args);
}

typedef void c_log_func(const char *, va_list);
static c_log_func *c_log_dispatch[3] = {
    c_log_debug,
    c_log_warn,
    c_log_error
};

void c_log(enum c_log_level level, const char *format, ...)
{
    va_list args;
    if (level >= c_log_active_level) {
        va_start(args, format);
        c_log_dispatch[level](format, args);
        va_end(args);
    }
}
