#ifndef CHUNKWM_PLUGIN_CVAR_H
#define CHUNKWM_PLUGIN_CVAR_H

#include <stddef.h>

struct cvar
{
    const char *Name;
    char *Value;
};

#define CHUNKWM_API_BROADCAST_FUNC(name) void name(const char *Plugin, const char *Event, void *Data, size_t Size)
typedef CHUNKWM_API_BROADCAST_FUNC(plugin_broadcast_func);

#define CHUNKWM_API_UPDATE_CVAR_FUNC(name) void name(const char *Name, char *Value)
typedef CHUNKWM_API_UPDATE_CVAR_FUNC(chunkwm_update_cvar_func);

#define CHUNKWM_API_ACQUIRE_CVAR_FUNC(name) char *name(const char *Name)
typedef CHUNKWM_API_ACQUIRE_CVAR_FUNC(chunkwm_acquire_cvar_func);

#define CHUNKWM_API_FIND_CVAR_FUNC(name) bool name(const char *Name)
typedef CHUNKWM_API_FIND_CVAR_FUNC(chunkwm_find_cvar_func);

struct chunkwm_api
{
    chunkwm_update_cvar_func *UpdateCVar;
    chunkwm_acquire_cvar_func *AcquireCVar;
    chunkwm_find_cvar_func *FindCVar;
    plugin_broadcast_func *Broadcast;
};

#endif
