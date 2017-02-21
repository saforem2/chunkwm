#ifndef PLUGIN_MISC_H
#define PLUGIN_MISC_H

#include <string.h>

inline bool
StringEquals(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

#endif
