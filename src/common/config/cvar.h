#ifndef CHUNKWM_COMMON_CVAR_H
#define CHUNKWM_COMMON_CVAR_H

#include <map>

#include "../misc/string.h"

enum cvar_type
{
    CVar_Int,
    CVar_Float,
    CVar_String,
};

struct cvar
{
    const char *Name;
    cvar_type Type;

    union
    {
        int Integer;
        float Float;
        char *String;
    };
};

typedef std::map<const char *, cvar *, string_comparator> cvar_map;
typedef cvar_map::iterator cvar_map_it;

bool BeginCVars();
void EndCVars();

// NOTE(koekeishiya): The caller is responsible for making sure that
// a cvar with 'name' does not already exist. The existing cvar pointer
// will be overwritten and is effectively a memory leak.
void CreateCVar(const char *Name, int Value);
void CreateCVar(const char *Name, float Value);
void CreateCVar(const char *Name, char *Value);

// NOTE(koekeishiya): If the cvar doesn't exist, it will be created.
void UpdateCVar(const char *Name, int Value);
void UpdateCVar(const char *Name, float Value);
void UpdateCVar(const char *Name, char *Value);

int CVarIntegerValue(const char *Name);
float CVarFloatingPointValue(const char *Name);
char *CVarStringValue(const char *Name);

bool CVarExists(const char *Name);

#endif
