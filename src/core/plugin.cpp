#include "plugin.h"
#include "cvar.h"
#include "clog.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <map>

#define internal static

internal std::map<const char *, loaded_plugin *, string_comparator> LoadedPlugins;
internal pthread_mutex_t LoadedPluginLock;

internal pthread_mutex_t Mutexes[chunkwm_export_count];
internal plugin_list ExportedPlugins[chunkwm_export_count];

internal chunkwm_api API = { UpdateCVarAPI,  AcquireCVarAPI, FindCVarAPI, ChunkwmBroadcast, (chunkwm_log*)c_log };

internal bool
VerifyPluginABI(plugin_details *Info)
{
    bool Result = (Info->ApiVersion == CHUNKWM_PLUGIN_API_VERSION);
    return Result;
}

internal void
PrintPluginDetails(plugin_details *Info)
{
    c_log(C_LOG_LEVEL_DEBUG,
          "Plugin Details\n"
          "API Version %d\n"
          "FileName '%s'\n"
          "PluginName '%s'\n"
          "PluginVersion '%s'\n",
           Info->ApiVersion,
           Info->FileName,
           Info->PluginName,
           Info->PluginVersion);
}

plugin_list *BeginPluginList(chunkwm_plugin_export Export)
{
    pthread_mutex_lock(&Mutexes[Export]);
    return &ExportedPlugins[Export];
}

void EndPluginList(chunkwm_plugin_export Export)
{
    pthread_mutex_unlock(&Mutexes[Export]);
}

internal void
SubscribeToEvent(plugin *Plugin, chunkwm_plugin_export Export)
{
    plugin_list *List = BeginPluginList(Export);

    plugin_list_iter It = List->find(Plugin);
    if (It == List->end()) {
       (*List)[Plugin] = true;
    }

    EndPluginList(Export);
}

internal void
UnsubscribeFromEvent(plugin *Plugin, chunkwm_plugin_export Export)
{
    plugin_list *List = BeginPluginList(Export);

    plugin_list_iter It = List->find(Plugin);
    if (It != List->end()) {
        List->erase(It);
    }

    EndPluginList(Export);
}

internal void
HookPlugin(loaded_plugin *LoadedPlugin)
{
    plugin *Plugin = LoadedPlugin->Plugin;
    if (Plugin->Subscriptions) {
        for (int Index = 0; Index < Plugin->SubscriptionCount; ++Index) {
            chunkwm_plugin_export *Export = Plugin->Subscriptions + Index;
            c_log(C_LOG_LEVEL_DEBUG,
                  "Plugin '%s' subscribed to '%s'\n",
                  LoadedPlugin->Info->PluginName,
                  chunkwm_plugin_export_str[*Export]);
            SubscribeToEvent(Plugin, *Export);
        }
    }
    Plugin->Run("chunkwm_events_subscribed", NULL);
}

internal void
UnhookPlugin(loaded_plugin *LoadedPlugin)
{
    plugin *Plugin = LoadedPlugin->Plugin;
    if (Plugin->Subscriptions) {
        for (int Index = 0; Index < Plugin->SubscriptionCount; ++Index) {
            chunkwm_plugin_export *Export = Plugin->Subscriptions + Index;
            c_log(C_LOG_LEVEL_DEBUG,
                  "Plugin '%s' unsubscribed from '%s'\n",
                  LoadedPlugin->Info->PluginName,
                  chunkwm_plugin_export_str[*Export]);
            UnsubscribeFromEvent(Plugin, *Export);
        }
    }
}

internal void
StoreLoadedPlugin(loaded_plugin *LoadedPlugin)
{
    BeginLoadedPluginList();
    LoadedPlugins[LoadedPlugin->Filename] = LoadedPlugin;
    EndLoadedPluginList();
}

internal loaded_plugin *
RemoveLoadedPlugin(const char *Filename)
{
    BeginLoadedPluginList();

    loaded_plugin *Result;
    if (LoadedPlugins.find(Filename) != LoadedPlugins.end()) {
        Result = LoadedPlugins[Filename];
        LoadedPlugins.erase(Filename);
    } else {
        Result = NULL;
    }

    EndLoadedPluginList();
    return Result;
}

internal bool
IsPluginLoaded(const char *Filename)
{
    BeginLoadedPluginList();
    bool Result = LoadedPlugins.find(Filename) != LoadedPlugins.end();
    EndLoadedPluginList();
    return Result;
}

plugin *GetPluginFromFilename(const char *Filename)
{
    BeginLoadedPluginList();

    int Length = strlen(Filename) + 3 + 1;
    char FilenameWithExtension[Length];
    snprintf(FilenameWithExtension, Length, "%s.so", Filename);

    plugin *Result;
    if (LoadedPlugins.find(FilenameWithExtension) != LoadedPlugins.end()) {
        Result = LoadedPlugins[FilenameWithExtension]->Plugin;
    } else {
        Result = NULL;
    }

    EndLoadedPluginList();
    return Result;
}

loaded_plugin_list *BeginLoadedPluginList()
{
    pthread_mutex_lock(&LoadedPluginLock);
    return &LoadedPlugins;
}

void EndLoadedPluginList()
{
    pthread_mutex_unlock(&LoadedPluginLock);
}

bool LoadPlugin(const char *Absolutepath, const char *Filename)
{
    bool Result = true;

    void *Handle;
    plugin_details *Info;
    plugin *Plugin;
    loaded_plugin *LoadedPlugin;

    if (IsPluginLoaded(Filename)) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: plugin '%s' is already running!\n", Absolutepath);
        goto already_loaded;
    }

    Handle = dlopen(Absolutepath, RTLD_LAZY);
    if (!Handle) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: dlopen '%s' failed!\n", Absolutepath);
        goto handle_err;
    }

    Info = (plugin_details *) dlsym(Handle, "Exports");
    if (!Info) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: dlsym '%s' plugin details missing!\n", Absolutepath);
        goto info_err;
    }

    if (!VerifyPluginABI(Info)) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: plugin '%s' ABI mismatch; expected %d, was %d\n",
              Info->PluginName, CHUNKWM_PLUGIN_API_VERSION, Info->ApiVersion);
        goto abi_err;
    }

    Plugin = Info->Initialize();
    PrintPluginDetails(Info);

    LoadedPlugin = (loaded_plugin *) malloc(sizeof(loaded_plugin));
    LoadedPlugin->Handle = Handle;
    LoadedPlugin->Plugin = Plugin;
    LoadedPlugin->Info = Info;

    if (!Plugin->Init(API)) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: plugin '%s' init failed!\n", Info->PluginName);
        goto plugin_init_err;
    }

    c_log(C_LOG_LEVEL_DEBUG, "chunkwm: plugin '%s' loaded!\n", Filename);
    LoadedPlugin->Filename = strdup(Filename);
    StoreLoadedPlugin(LoadedPlugin);
    HookPlugin(LoadedPlugin);
    goto out;

plugin_init_err:
    free(LoadedPlugin);

abi_err:
info_err:
    dlclose(Handle);

handle_err:
already_loaded:
    Result = false;

out:
    return Result;
}

bool UnloadPlugin(const char *Absolutepath, const char *Filename)
{
    bool Result = false;

    loaded_plugin *LoadedPlugin = RemoveLoadedPlugin(Filename);
    if (LoadedPlugin && LoadedPlugin->Handle) {
        UnhookPlugin(LoadedPlugin);

        plugin *Plugin = LoadedPlugin->Plugin;
        Plugin->DeInit();

        Result = dlclose(LoadedPlugin->Handle) == 0;

#if 0
        /*
         * NOTE(koekeishiya): The objective-c runtime calls dlopen
         * and increments the reference count of the handle.
         * We decrement the counter to 0 to unload the library.
         */
        while (dlopen(Absolutepath, RTLD_NOLOAD)) {
            dlclose(LoadedPlugin->Handle);
            dlclose(LoadedPlugin->Handle);
        }

        /*
         * NOTE(koekeishiya): This workaround no longer works on MacOS Sierra.
         * Any plugin that uses the objective-c runtime cannot be dynamically reloaded !!!
         *
         * Unloading and reloading the plugin will still work properly, however, it cannot
         * be used to load NEW code, if the plugin has been modified and recompiled !!!
         */
#endif

        c_log(C_LOG_LEVEL_DEBUG, "chunkwm: plugin '%s' unloaded!\n", Filename);

        free(LoadedPlugin->Filename);
        free(LoadedPlugin);
    }

    return Result;
}

bool BeginPlugins()
{
    for (int Index = 0; Index < chunkwm_export_count; ++Index) {
        if (pthread_mutex_init(&Mutexes[Index], NULL) != 0) {
            return false;
        }
    }

    return (pthread_mutex_init(&LoadedPluginLock, NULL) == 0);
}

void DestroyPluginFS(plugin_fs *PluginFS)
{
    free(PluginFS->Absolutepath);
    free(PluginFS->Filename);
}
