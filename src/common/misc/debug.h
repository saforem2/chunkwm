#ifndef CHUNKWM_COMMON_DEBUG_H
#define CHUNKWM_COMMON_DEBUG_H

#ifdef CHUNKWM_DEBUG
#include <stdio.h>
#define DEBUG_PRINT(fmt, args...) do { printf(fmt, ## args); } while(0)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

#endif
