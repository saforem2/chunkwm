#include "clog.h"

#include <stdio.h>
#include <stdarg.h>

enum c_log_level c_log_active_level = C_LOG_LEVEL_ERROR;
FILE *c_log_output_file = stdout;

const char *c_log_level_str[] = {
    "DEBUG", "WARN", "ERROR"
};

void c_log(enum c_log_level level, const char *format, ...)
{
    va_list args;
    time_t time_epoch;
    char time_buffer[32];
    struct tm *local_time;

    if (level >= c_log_active_level) {
        time_epoch = time(NULL);
        local_time = localtime(&time_epoch);
        time_buffer[strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time)] = '\0';
        fprintf(c_log_output_file, "%s %-5s: ", time_buffer, c_log_level_str[level]);

        va_start(args, format);
        vfprintf(c_log_output_file, format, args);
        va_end(args);

        fflush(c_log_output_file);
    }
}
