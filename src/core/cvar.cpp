#include "cvar.h"

#include <stdlib.h>
#include <pthread.h>

#include "../common/misc/assert.h"

#define internal static

extern chunkwm_api API;

internal cvar_map CVars;
internal pthread_mutex_t CVarsLock;

internal cvar *
_FindCVar(const char *Name)
{
    cvar_map_it It = CVars.find(Name);
    return It != CVars.end() ? It->second : NULL;
}

internal cvar *
_CreateCVar(const char *Name, char *Value)
{
    cvar *Var = (cvar *) malloc(sizeof(cvar));

    Var->Name = strdup(Name);
    Var->Value = strdup(Value);

    return Var;
}

bool BeginCVars()
{
    BeginCVars(&API);
    return pthread_mutex_init(&CVarsLock, NULL) == 0;
}

void EndCVars()
{
    for(cvar_map_it It = CVars.begin();
        It != CVars.end();
        ++It)
    {
        cvar *Var = It->second;

        free((char *) Var->Name);
        free(Var->Value);
        free(Var);
    }

    CVars.clear();
    pthread_mutex_destroy(&CVarsLock);
}

// NOTE(koekeishiya): API - Exposed to plugins through pointer
void UpdateCVarAPI(const char *Name, char *Value)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *Var = _FindCVar(Name);
    if(Var)
    {
        ASSERT(Var->Value);
        free(Var->Value);
        Var->Value = strdup(Value);
    }
    else
    {
        cvar *Var = _CreateCVar(Name, Value);
        CVars[Var->Name] = Var;
    }
    pthread_mutex_unlock(&CVarsLock);
}

// NOTE(koekeishiya): API - Exposed to plugins through pointer
char *AcquireCVarAPI(const char *Name)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *CVar = _FindCVar(Name);
    char *Result = CVar ? CVar->Value : NULL;
    pthread_mutex_unlock(&CVarsLock);
    return Result;
}

// NOTE(koekeishiya): API - Exposed to plugins through pointer
bool FindCVarAPI(const char *Name)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *CVar = _FindCVar(Name);
    pthread_mutex_unlock(&CVarsLock);
    return CVar != NULL;
}
