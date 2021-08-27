#ifndef CHUNKWM_CORE_CLOG_H
#define CHUNKWM_CORE_CLOG_H

#include <stdio.h>

enum c_log_level
{
    C_LOG_LEVEL_DEBUG   =  0,
    C_LOG_LEVEL_PROFILE =  1,
    C_LOG_LEVEL_WARN    =  2,
    C_LOG_LEVEL_ERROR   =  3,
    C_LOG_LEVEL_NONE    = 10,
};

extern enum c_log_level c_log_active_level;
extern FILE *c_log_output_file;
void c_log(enum c_log_level level, const char *format, ...);

#endif
