#ifndef CHUNKWM_COMMON_CVAR_H
#define CHUNKWM_COMMON_CVAR_H

struct chunkwm_api;
void BeginCVars(chunkwm_api *Api);

bool CVarExists(const char *Name);

void UpdateCVar(const char *Name, int Value);
void UpdateCVar(const char *Name, unsigned Value);
void UpdateCVar(const char *Name, float Value);
void UpdateCVar(const char *Name, char *Value);

void CreateCVar(const char *Name, int Value);
void CreateCVar(const char *Name, unsigned Value);
void CreateCVar(const char *Name, float Value);
void CreateCVar(const char *Name, char *Value);

int CVarIntegerValue(const char *Name);
int CVarUnsignedValue(const char *Name);
float CVarFloatingPointValue(const char *Name);
char *CVarStringValue(const char *Name);

#endif
