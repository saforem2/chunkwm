#ifndef CHUNKWM_COMMON_ASSERT_H
#define CHUNKWM_COMMON_ASSERT_H

#ifdef CHUNKWM_DEBUG
#include <stdio.h>
#define ASSERT(Condition) do { if(!(Condition)) {\
    printf("#%d:%s:%s: assert failed '%s'\n",        \
            __LINE__, __FILE__, __FUNCTION__, #Condition); \
    *(int volatile *)0 = 0;                      \
    } } while(0)
#else
#define ASSERT(Ignored)
#endif

#endif
