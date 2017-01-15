#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <pthread.h>

#include "plugin.h"
#define internal static

struct loaded_plugin
{
    void *Handle;
    plugin *Plugin;
    plugin_details *Info;
};

internal pthread_mutex_t Mutexes[chunkwm_export_application_end];
internal plugin_list ExportedPlugins[chunkwm_export_application_end];

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
        chunkwm_plugin_export *Export = Plugin->Subscriptions;
        while(*Export != chunkwm_export_application_end)
        {
            switch(*Export)
            {
                case chunkwm_export_application_launched:
                case chunkwm_export_application_terminated:
                case chunkwm_export_application_hidden:
                case chunkwm_export_application_unhidden:
                {
                    printf("Plugin '%s' subscribed to '%s'\n",
                           LoadedPlugin->Info->PluginName,
                           chunkwm_plugin_export_str[*Export]);
                    SubscribeToEvent(Plugin, *Export);
                } break;
                default:
                {
                    fprintf(stderr,
                            "Plugin '%s' contains invalid subscription!\n",
                            LoadedPlugin->Info->PluginName);
                } break;
            }

            ++Export;
        }
    }
}

internal void
UnhookPlugin(loaded_plugin *LoadedPlugin)
{
    plugin *Plugin = LoadedPlugin->Plugin;
    if(Plugin->Subscriptions)
    {
        chunkwm_plugin_export *Export = Plugin->Subscriptions;
        while(*Export != chunkwm_export_application_end)
        {
            printf("Plugin '%s' unsubscribed from '%s'\n",
                   LoadedPlugin->Info->PluginName,
                   chunkwm_plugin_export_str[*Export]);
            UnsubscribeFromEvent(Plugin, *Export);
            ++Export;
        }
    }
}

bool LoadPlugin(const char *File, loaded_plugin *LoadedPlugin)
{
    void *Handle = dlopen(File, RTLD_LAZY);
    if(Handle)
    {
        plugin_details *Info = (plugin_details *) dlsym(Handle, "Exports");
        if(Info)
        {
            if(VerifyPluginABI(Info))
            {
                plugin *Plugin = Info->Initialize();
                LoadedPlugin->Handle = Handle;
                LoadedPlugin->Plugin = Plugin;
                LoadedPlugin->Info = Info;
                PrintPluginDetails(Info);

                HookPlugin(LoadedPlugin);
                Plugin->Init(Plugin);
                return true;
            }
            else
            {
                dlclose(Handle);
                fprintf(stderr, "Plugin '%s' ABI mismatch; expected %d, was %d\n",
                        Info->PluginName, CHUNKWM_PLUGIN_API_VERSION, Info->ApiVersion);
            }
        }
        else
        {
            dlclose(Handle);
            fprintf(stderr, "Plugin details missing!\n");
        }
    }

    return false;
}

bool UnloadPlugin(loaded_plugin *LoadedPlugin)
{
    bool Result = false;
    if(LoadedPlugin->Handle)
    {
        UnhookPlugin(LoadedPlugin);

        plugin *Plugin = LoadedPlugin->Plugin;
        Plugin->DeInit(Plugin);

        Result = dlclose(LoadedPlugin->Handle) == 0;
    }

    return Result;
}

bool BeginPlugins()
{
    for(int Index = 0;
        Index < chunkwm_export_application_end;
        ++Index)
    {
        if(pthread_mutex_init(&Mutexes[Index], NULL) != 0)
        {
            return false;
        }
    }

    loaded_plugin LoadedPlugin;
    if(LoadPlugin("plugins/testplugin.so", &LoadedPlugin))
    {
#if 0
        UnloadPlugin(&LoadedPlugin);
#endif
    }

    loaded_plugin LoadedPluginX;
    if(LoadPlugin("plugins/testpluginx.so", &LoadedPluginX))
    {
#if 0
        UnloadPlugin(&LoadedPlugin);
#endif
    }


    return true;
}
