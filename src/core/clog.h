#ifndef CHUNKWM_COMMON_CLOG_H
#define CHUNKWM_COMMON_CLOG_H

enum c_log_level
{
    C_LOG_LEVEL_DEBUG =  0,
    C_LOG_LEVEL_WARN  =  1,
    C_LOG_LEVEL_ERROR =  2,
    C_LOG_LEVEL_NONE  = 10,
};

extern enum c_log_level c_log_active_level;
void c_log(enum c_log_level level, const char *format, ...);

#endif
