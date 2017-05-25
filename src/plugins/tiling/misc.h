#ifndef PLUGIN_MISC_H
#define PLUGIN_MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline bool
StringEquals(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

inline char *
ReadFile(const char *Absolutepath)
{
    char *Contents = NULL;
    FILE *Descriptor = fopen(Absolutepath, "r");

    if(Descriptor)
    {
        fseek(Descriptor, 0, SEEK_END);
        unsigned Length = ftell(Descriptor);
        fseek(Descriptor, 0, SEEK_SET);

        Contents = (char *) malloc(Length + 1);
        fread(Contents, Length, 1, Descriptor);
        Contents[Length] = '\0';

        fclose(Descriptor);
    }

    return Contents;
}

#endif
