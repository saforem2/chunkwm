#include "cvar.h"
#include "../../api/plugin_cvar.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define internal static

internal chunkwm_api *ChunkwmAPI;

void BeginCVars(chunkwm_api *API)
{
    ChunkwmAPI = API;
}

bool CVarExists(const char *Name)
{
    return ChunkwmAPI->FindCVar(Name);
}

void UpdateCVar(const char *Name, int Value)
{
    char *String = (char *) malloc(256);
    memset(String, 0, 256);
    snprintf(String, 256, "%d", Value);
    ChunkwmAPI->UpdateCVar(Name, String);
    free(String);
}

void UpdateCVar(const char *Name, unsigned Value)
{
    char *String = (char *) malloc(256);
    memset(String, 0, 256);
    snprintf(String, 256, "%x", Value);
    ChunkwmAPI->UpdateCVar(Name, String);
    free(String);
}

void UpdateCVar(const char *Name, float Value)
{
    char *String = (char *) malloc(256);
    memset(String, 0, 256);
    snprintf(String, 256, "%f", Value);
    ChunkwmAPI->UpdateCVar(Name, String);
    free(String);
}

void UpdateCVar(const char *Name, char *Value)
{
    ChunkwmAPI->UpdateCVar(Name, Value);
}

void CreateCVar(const char *Name, int Value)
{
    if(CVarExists(Name)) return;
    UpdateCVar(Name, Value);
}

void CreateCVar(const char *Name, unsigned Value)
{
    if(CVarExists(Name)) return;
    UpdateCVar(Name, Value);
}

void CreateCVar(const char *Name, float Value)
{
    if(CVarExists(Name)) return;
    UpdateCVar(Name, Value);
}

void CreateCVar(const char *Name, char *Value)
{
    if(CVarExists(Name)) return;
    UpdateCVar(Name, Value);
}

int CVarIntegerValue(const char *Name)
{
    char *String = ChunkwmAPI->AcquireCVar(Name);
    int Result = 0;
    sscanf(String, "%d", &Result);
    return Result;
}

int CVarUnsignedValue(const char *Name)
{
    char *String = ChunkwmAPI->AcquireCVar(Name);
    unsigned Result = 0;
    sscanf(String, "%x", &Result);
    return Result;
}

float CVarFloatingPointValue(const char *Name)
{
    char *String = ChunkwmAPI->AcquireCVar(Name);
    float Result = 0.0f;
    sscanf(String, "%f", &Result);
    return Result;
}

char *CVarStringValue(const char *Name)
{
    return ChunkwmAPI->AcquireCVar(Name);
}
