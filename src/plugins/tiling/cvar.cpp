#include "cvar.h"

#include <stdlib.h>
#include <pthread.h>

#define internal static

internal cvar_map CVars;
internal pthread_mutex_t CVarsLock;

void CreateCVar(const char *Name, int Value)
{
    cvar *Var = (cvar *) malloc(sizeof(cvar));

    Var->Name = strdup(Name);
    Var->Type = CVar_Int;
    Var->Integer = Value;

    pthread_mutex_lock(&CVarsLock);
    CVars[Var->Name] = Var;
    pthread_mutex_unlock(&CVarsLock);
}

void CreateCVar(const char *Name, float Value)
{
    cvar *Var = (cvar *) malloc(sizeof(cvar));

    Var->Name = strdup(Name);
    Var->Type = CVar_Float;
    Var->Float = Value;

    pthread_mutex_lock(&CVarsLock);
    CVars[Var->Name] = Var;
    pthread_mutex_unlock(&CVarsLock);
}

void CreateCVar(const char *Name, char *Value)
{
    cvar *Var = (cvar *) malloc(sizeof(cvar));

    Var->Name = strdup(Name);
    Var->Type = CVar_String;
    Var->String = Value;

    pthread_mutex_lock(&CVarsLock);
    CVars[Var->Name] = Var;
    pthread_mutex_unlock(&CVarsLock);
}

internal cvar *
FindCVar(const char *Name)
{
    cvar_map_it It = CVars.find(Name);
    return It != CVars.end() ? It->second : NULL;
}

void UpdateCVar(const char *Name, int Value)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Var->Integer = Value;
    }
    pthread_mutex_unlock(&CVarsLock);
}

void UpdateCVar(const char *Name, float Value)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Var->Float = Value;
    }
    pthread_mutex_unlock(&CVarsLock);
}

void UpdateCVar(const char *Name, char *Value)
{
    pthread_mutex_lock(&CVarsLock);
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Var->String = Value;
    }
    pthread_mutex_unlock(&CVarsLock);
}

int CVarIntegerValue(const char *Name)
{
    pthread_mutex_lock(&CVarsLock);

    int Result;
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Result = Var->Integer;
    }
    else
    {
        Result = 0;
    }

    pthread_mutex_unlock(&CVarsLock);
    return Result;
}

float CVarFloatingPointValue(const char *Name)
{
    pthread_mutex_lock(&CVarsLock);

    float Result;
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Result = Var->Float;
    }
    else
    {
        Result = 0.0f;
    }

    pthread_mutex_unlock(&CVarsLock);
    return Result;
}

char *CVarStringValue(const char *Name)
{
    pthread_mutex_lock(&CVarsLock);

    char *Result;
    cvar *Var = FindCVar(Name);
    if(Var)
    {
        Result = Var->String;
    }
    else
    {
        Result = NULL;
    }

    pthread_mutex_unlock(&CVarsLock);
    return Result;
}

bool BeginCVars()
{
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
        if(Var->Type == CVar_String)
        {
            free(Var->String);
        }
        free(Var);
    }

    CVars.clear();
    pthread_mutex_destroy(&CVarsLock);
}
