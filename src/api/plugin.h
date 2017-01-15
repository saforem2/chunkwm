#ifndef CHUNKWM_PLUGIN_H
#define CHUNKWM_PLUGIN_H

#define CHUNKWM_EXTERN extern "C"

// NOTE(koekeishiya): Increment upon ABI breaking changes!
#define CHUNKWM_PLUGIN_API_VERSION 1

// NOTE(koekeishiya): This struct is defined externally.
struct plugin;

CHUNKWM_EXTERN typedef plugin *(*plugin_func)();
struct plugin_details
{
    int ApiVersion;
    const char *FileName;
    const char *PluginName;
    const char *PluginVersion;
    plugin_func Initialize;
};

#define CHUNKWM_PLUGIN(PluginName, PluginVersion)                \
      CHUNKWM_EXTERN                                             \
      {                                                          \
          plugin *GetPlugin()                                    \
          {                                                      \
              static plugin Singleton;                           \
              InitPluginVTable(&Singleton);                      \
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
