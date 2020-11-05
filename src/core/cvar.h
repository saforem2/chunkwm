#ifndef CHUNKWM_CORE_CVAR_H
#define CHUNKWM_CORE_CVAR_H

#include <map>

#include "../common/config/cvar.h"
#include "../common/misc/string.h"

typedef std::map<const char *, cvar *, string_comparator> cvar_map;
typedef cvar_map::iterator cvar_map_it;

bool BeginCVars();
void EndCVars();

// NOTE(koekeishiya): API - Exposed to plugins through pointer
void UpdateCVarAPI(const char *Name, char *Value);

// NOTE(koekeishiya): API - Exposed to plugins through pointer
char *AcquireCVarAPI(const char *Name);

// NOTE(koekeishiya): API - Exposed to plugins through pointer
bool FindCVarAPI(const char *Name);

#endif
