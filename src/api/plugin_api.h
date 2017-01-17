#ifndef CHUNKWM_PLUGIN_API_H
#define CHUNKWM_PLUGIN_API_H

#define CHUNKWM_EXTERN extern "C"

// NOTE(koekeishiya): Increment upon ABI breaking changes!
#define CHUNKWM_PLUGIN_API_VERSION 1

// NOTE(koekeishiya): Forward-declare struct
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

#define PLUGIN_INIT_FUNC(name) bool name(plugin *Plugin)
typedef PLUGIN_INIT_FUNC(plugin_init_func);

#define PLUGIN_DEINIT_FUNC(name) void name(plugin *Plugin)
typedef PLUGIN_DEINIT_FUNC(plugin_deinit_func);

#define PLUGIN_MAIN_FUNC(name) \
    bool name(plugin *Plugin,\
            const char *Node,\
            const char *Data,\
            unsigned int DataSize)
typedef PLUGIN_MAIN_FUNC(plugin_main_func);


static const char *chunkwm_plugin_export_str[] =
{
    "chunkwm_export_application_launched",
    "chunkwm_export_application_terminated",
    "chunkwm_export_application_activated",
    "chunkwm_export_application_deactivated",
    "chunkwm_export_application_hidden",
    "chunkwm_export_application_unhidden",

    "chunkwm_export_space_changed",

    "chunkwm_export_end",
};
enum chunkwm_plugin_export
{
    chunkwm_export_application_launched,
    chunkwm_export_application_terminated,
    chunkwm_export_application_activated,
    chunkwm_export_application_deactivated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,

    chunkwm_export_space_changed,

    chunkwm_export_end,
};

struct plugin
{
    plugin_init_func *Init;
    plugin_deinit_func *DeInit;
    plugin_main_func *Run;

    chunkwm_plugin_export *Subscriptions;
};

#endif
