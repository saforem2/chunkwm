#ifndef CHUNKWM_PLUGIN_API_H
#define CHUNKWM_PLUGIN_API_H

#include "plugin_export.h"

#define CHUNKWM_EXTERN extern "C"

// NOTE(koekeishiya): Increment upon ABI breaking changes!
#define CHUNKWM_PLUGIN_API_VERSION 1

//NOTE(koekeishiya): Forward-declare struct
struct plugin;

#define PLUGIN_BOOL_FUNC(name) bool name(plugin *Plugin)
typedef PLUGIN_BOOL_FUNC(plugin_bool_func);

#define PLUGIN_VOID_FUNC(name) void name(plugin *Plugin)
typedef PLUGIN_VOID_FUNC(plugin_void_func);

#define PLUGIN_MAIN_FUNC(name) \
    bool name(plugin *Plugin,\
            const char *Node,\
            const char *Data,\
            unsigned int DataSize)
typedef PLUGIN_MAIN_FUNC(plugin_main_func);

struct plugin
{
    plugin_bool_func *Init;
    plugin_void_func *DeInit;
    plugin_main_func *Run;

    chunkwm_plugin_export *Subscriptions;
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

#define CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain) \
    void InitPluginVTable(plugin *Plugin)\
    {\
        Plugin->Init = PluginInit;\
        Plugin->DeInit = PluginDeInit;\
        Plugin->Run = PluginMain;\
    }

#define ArrayCount(array) (sizeof(array) / sizeof(*(array)))

#define CHUNKWM_PLUGIN_SUBSCRIBE(SubscriptionArray)              \
    void InitPluginSubscriptions(plugin *Plugin)                 \
    {                                                            \
        int Count = ArrayCount(SubscriptionArray);               \
        Plugin->Subscriptions =                                  \
            (chunkwm_plugin_export *) malloc((Count + 1) *       \
                    sizeof(chunkwm_plugin_export));              \
        Plugin->Subscriptions[Count] = chunkwm_export_end;       \
        for(int Index = 0; Index < Count; ++Index)               \
        {                                                        \
            Plugin->Subscriptions[Index] =                       \
                SubscriptionArray[Index];                        \
        }                                                        \
     }

#define CHUNKWM_PLUGIN(PluginName, PluginVersion)                \
      CHUNKWM_EXTERN                                             \
      {                                                          \
          plugin *GetPlugin()                                    \
          {                                                      \
              static plugin Singleton;                           \
              InitPluginVTable(&Singleton);                      \
              InitPluginSubscriptions(&Singleton);               \
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
