#ifndef CHUNKWM_PLUGIN_API_H
#define CHUNKWM_PLUGIN_API_H

#include "plugin_export.h"
#include "plugin_cvar.h"

#define CHUNKWM_EXTERN extern "C"

// NOTE(koekeishiya): Increment upon ABI breaking changes!
#define CHUNKWM_PLUGIN_API_VERSION 5

// NOTE(koekeishiya): Forward-declare struct
struct plugin;

#define PLUGIN_BOOL_FUNC(name) bool name(chunkwm_api ChunkwmAPI)
typedef PLUGIN_BOOL_FUNC(plugin_bool_func);

#define PLUGIN_VOID_FUNC(name) void name()
typedef PLUGIN_VOID_FUNC(plugin_void_func);

#define PLUGIN_MAIN_FUNC(name)   \
    bool name(const char *Node,  \
              void *Data)
typedef PLUGIN_MAIN_FUNC(plugin_main_func);

struct plugin
{
    plugin_bool_func *Init;
    plugin_void_func *DeInit;
    plugin_main_func *Run;

    chunkwm_plugin_export *Subscriptions;
    unsigned SubscriptionCount;
};

CHUNKWM_EXTERN typedef plugin *(*plugin_func)();
struct plugin_details
{
    int ApiVersion;
    const char *FileName;
    const char *PluginName;
    const char *PluginVersion;
    plugin_func Initialize;
};

#define CHUNKWM_PLUGIN_VTABLE(PlInit, PlDeInit, PlMain)          \
    void InitPluginVTable(plugin *Plugin)                        \
    {                                                            \
        Plugin->Init = PlInit;                                   \
        Plugin->DeInit = PlDeInit;                               \
        Plugin->Run = PlMain;                                    \
    }

#define CHUNKWM_PLUGIN_SUBSCRIBE(Sub)                            \
    void InitPluginSubscriptions(plugin *Plugin)                 \
    {                                                            \
        Plugin->SubscriptionCount = sizeof(Sub) / sizeof(*Sub);  \
        Plugin->Subscriptions = Sub;                             \
    }

#define CHUNKWM_PLUGIN(PluginName, PluginVersion)                \
      CHUNKWM_EXTERN                                             \
      {                                                          \
          plugin *GetPlugin()                                    \
          {                                                      \
              static plugin Singleton;                           \
              static bool Initialized = false;                   \
              if(!Initialized)                                   \
              {                                                  \
                  InitPluginVTable(&Singleton);                  \
                  InitPluginSubscriptions(&Singleton);           \
                  Initialized = true;                            \
              }                                                  \
              return &Singleton;                                 \
          }                                                      \
          plugin_details Exports =                               \
          {                                                      \
              CHUNKWM_PLUGIN_API_VERSION,                        \
              __FILE__,                                          \
              PluginName,                                        \
              PluginVersion,                                     \
              GetPlugin,                                         \
          };                                                     \
      }

#endif
