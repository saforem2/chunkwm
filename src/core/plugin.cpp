#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>

#include <map>

#include "plugin.h"
#include "../common/misc/string.h"

#define internal static

struct loaded_plugin
{
    char *Filename;
    void *Handle;
    plugin *Plugin;
    plugin_details *Info;
};

internal std::map<const char *, loaded_plugin *, string_comparator> LoadedPlugins;

internal pthread_mutex_t Mutexes[chunkwm_export_count];
internal plugin_list ExportedPlugins[chunkwm_export_count];

internal bool
VerifyPluginABI(plugin_details *Info)
{
    bool Result = (Info->ApiVersion == CHUNKWM_PLUGIN_API_VERSION);
    return Result;
}

internal void
PrintPluginDetails(plugin_details *Info)
{
    printf("Plugin Details\n"
           "API Version %d\n"
           "FileName '%s'\n"
           "PluginName '%s'\n"
           "PluginVersion '%s'\n\n",
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
    if(It == List->end())
    {
       (*List)[Plugin] = true;
    }

    EndPluginList(Export);
}

internal void
UnsubscribeFromEvent(plugin *Plugin, chunkwm_plugin_export Export)
{
    plugin_list *List = BeginPluginList(Export);

    plugin_list_iter It = List->find(Plugin);
    if(It != List->end())
    {
        List->erase(It);
    }

    EndPluginList(Export);
}

internal void
HookPlugin(loaded_plugin *LoadedPlugin)
{
    plugin *Plugin = LoadedPlugin->Plugin;
    if(Plugin->Subscriptions)
    {
        for(int Index = 0;
            Index < Plugin->SubscriptionCount;
            ++Index)
        {
            chunkwm_plugin_export *Export = Plugin->Subscriptions + Index;
            printf("Plugin '%s' subscribed to '%s'\n",
                   LoadedPlugin->Info->PluginName,
                   chunkwm_plugin_export_str[*Export]);
            SubscribeToEvent(Plugin, *Export);
        }
    }
}

internal void
UnhookPlugin(loaded_plugin *LoadedPlugin)
{
    plugin *Plugin = LoadedPlugin->Plugin;
    if(Plugin->Subscriptions)
    {
        for(int Index = 0;
            Index < Plugin->SubscriptionCount;
            ++Index)
        {
            chunkwm_plugin_export *Export = Plugin->Subscriptions + Index;
            printf("Plugin '%s' unsubscribed from '%s'\n",
                   LoadedPlugin->Info->PluginName,
                   chunkwm_plugin_export_str[*Export]);
            UnsubscribeFromEvent(Plugin, *Export);
        }
    }
}

internal void
StoreLoadedPlugin(loaded_plugin *LoadedPlugin)
{
    LoadedPlugins[LoadedPlugin->Filename] = LoadedPlugin;
}

internal loaded_plugin *
RemoveLoadedPlugin(const char *Filename)
{
    loaded_plugin *Result;
    if(LoadedPlugins.find(Filename) != LoadedPlugins.end())
    {
        Result = LoadedPlugins[Filename];
        LoadedPlugins.erase(Filename);
    }
    else
    {
        Result = NULL;
    }

    return Result;
}

bool LoadPlugin(const char *Absolutepath, const char *Filename)
{
    void *Handle = dlopen(Absolutepath, RTLD_LAZY);
    if(Handle)
    {
        plugin_details *Info = (plugin_details *) dlsym(Handle, "Exports");
        if(Info)
        {
            if(VerifyPluginABI(Info))
            {
                plugin *Plugin = Info->Initialize();
#ifdef CHUNKWM_DEBUG
                PrintPluginDetails(Info);
#endif

                loaded_plugin *LoadedPlugin = (loaded_plugin *) malloc(sizeof(loaded_plugin));
                LoadedPlugin->Handle = Handle;
                LoadedPlugin->Plugin = Plugin;
                LoadedPlugin->Info = Info;

                if(Plugin->Init())
                {
                    printf("chunkwm: load plugin '%s'\n", Filename);
                    LoadedPlugin->Filename = strdup(Filename);
                    StoreLoadedPlugin(LoadedPlugin);
                    HookPlugin(LoadedPlugin);
                    return true;
                }
                else
                {
                    free(LoadedPlugin);
                    fprintf(stderr, "Plugin '%s' init failed!\n", Info->PluginName);
                    dlclose(Handle);
                }
            }
            else
            {
                fprintf(stderr, "Plugin '%s' ABI mismatch; expected %d, was %d\n",
                        Info->PluginName, CHUNKWM_PLUGIN_API_VERSION, Info->ApiVersion);
                dlclose(Handle);
            }
        }
        else
        {
            fprintf(stderr, "dlsym '%s' plugin details missing!\n", Absolutepath);
            dlclose(Handle);
        }
    }
    else
    {
        fprintf(stderr, "dlopen '%s' failed!\n", Absolutepath);
    }

    return false;
}

bool UnloadPlugin(const char *Absolutepath, const char *Filename)
{
    bool Result = false;

    loaded_plugin *LoadedPlugin = RemoveLoadedPlugin(Filename);
    if(LoadedPlugin && LoadedPlugin->Handle)
    {
        printf("chunkwm: unload plugin '%s'\n", Filename);
        UnhookPlugin(LoadedPlugin);

        plugin *Plugin = LoadedPlugin->Plugin;
        Plugin->DeInit();

        Result = dlclose(LoadedPlugin->Handle) == 0;

        /* NOTE(koekeishiya): The objective-c runtime calls dlopen
         * and increments the reference count of the handle.
         * We decrement the counter to 0 to unload the library. */
        while(dlopen(Absolutepath, RTLD_NOLOAD))
        {
            dlclose(LoadedPlugin->Handle);
            dlclose(LoadedPlugin->Handle);
        }

        free(LoadedPlugin->Filename);
        free(LoadedPlugin);
    }

    return Result;
}

/* NOTE(koekeishiya):
 * Success                      =  0
 * Directory does not exist     = -1
 * Failed to initialize mutexes = -2 */
int BeginPlugins(const char *Directory)
{
    for(int Index = 0;
        Index < chunkwm_export_count;
        ++Index)
    {
        if(pthread_mutex_init(&Mutexes[Index], NULL) != 0)
        {
            return -2;
        }
    }

    DIR *Handle = opendir(Directory);
    if(!Handle)
    {
        return -1;
    }

    struct dirent *Entry;
    while((Entry = readdir(Handle)))
    {
        char *File = Entry->d_name;
        char *Extension = strrchr(File, '.');
        if((Extension) && (strcmp(Extension, ".so") == 0))
        {
            size_t DirectoryLength = strlen(Directory);
            size_t FilenameLength = strlen(Entry->d_name);
            size_t TotalLength = DirectoryLength + 1 + FilenameLength;

            char *Absolutepath = (char *) malloc(TotalLength + 1);
            Absolutepath[TotalLength] = '\0';

            memcpy(Absolutepath, Directory, DirectoryLength);
            memset(Absolutepath + DirectoryLength, '/', 1);
            memcpy(Absolutepath + DirectoryLength + 1, Entry->d_name, FilenameLength);

            LoadPlugin(Absolutepath, Entry->d_name);
            free(Absolutepath);
        }
    }

    closedir(Handle);
    return 0;
}
