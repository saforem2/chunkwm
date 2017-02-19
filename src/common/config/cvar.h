#ifndef PLUGIN_CVAR_H
#define PLUGIN_CVAR_H

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

void CreateCVar(const char *Name, int Value);
void CreateCVar(const char *Name, float Value);
void CreateCVar(const char *Name, char *Value);

void UpdateCVar(const char *Name, int Value);
void UpdateCVar(const char *Name, float Value);
void UpdateCVar(const char *Name, char *Value);

int CVarIntegerValue(const char *Name);
float CVarFloatingPointValue(const char *Name);
char *CVarStringValue(const char *Name);

#endif
