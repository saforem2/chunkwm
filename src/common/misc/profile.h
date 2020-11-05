#ifndef CHUNKWM_COMMON_PROFILE_H
#define CHUNKWM_COMMON_PROFILE_H

#ifdef CHUNKWM_PROFILE
#include <time.h>
#define BEGIN_TIMED_BLOCK() \
    clock_t timed_block_begin = clock()
#define END_TIMED_BLOCK() \
    clock_t timed_block_end = clock(); \
    double timed_block_elapsed = ((timed_block_end - timed_block_begin) / (double)CLOCKS_PER_SEC) * 1000.0f; \
    c_log(C_LOG_LEVEL_PROFILE, "#%d:%s:%s = %.8fms\n", __LINE__, __FILE__, __FUNCTION__, timed_block_elapsed)
#else
#define BEGIN_TIMED_BLOCK()
#define END_TIMED_BLOCK()
#endif

#endif
