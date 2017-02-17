#ifndef CHUNKWM_COMMON_STRING_H
#define CHUNKWM_COMMON_STRING_H

#include <string.h>

struct string_comparator {
bool operator()(const char *A, const char *B) const
{
        return strcmp(A, B) < 0;
}
};

#endif
