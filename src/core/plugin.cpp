#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>

#include "../api/plugin_api.h"
#define internal static

struct loaded_plugin
{
    void *Handle;
    plugin *Plugin;
    plugin_details *Info;
};

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
                {
                    printf("Plugin '%s' subscribed to chunkwm_export_application_launched\n",
                           LoadedPlugin->Info->PluginName);
                } break;
                case chunkwm_export_application_terminated:
                {
                    printf("Plugin '%s' subscribed to chunkwm_export_application_terminated\n",
                           LoadedPlugin->Info->PluginName);
                } break;
                case chunkwm_export_application_hidden:
                {
                    printf("Plugin '%s' subscribed to chunkwm_export_application_hidden\n",
                           LoadedPlugin->Info->PluginName);
                } break;
                case chunkwm_export_application_unhidden:
                {
                    printf("Plugin '%s' subscribed to chunkwm_export_application_unhidden\n",
                           LoadedPlugin->Info->PluginName);
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

#if 0
inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

// TODO(koekeishiya): ??
void *SearchPluginVTable(plugin *Plugin, const char *Search)
{
    plugin_vtable *VTable = Plugin->VTable;
    while(VTable->Func)
    {
        if(StringsAreEqual(VTable->Name, Search))
        {
            return (void *) VTable->Func;
        }

        ++VTable;
    }

    return NULL;
}
#endif

bool BeginPlugins()
{
    loaded_plugin LoadedPlugin;
    if(LoadPlugin("plugins/testplugin.so", &LoadedPlugin))
    {
#if 0
        typedef void (*FuncPtr)();
        FuncPtr Func = (FuncPtr) SearchPluginVTable(LoadedPlugin.Plugin, "Test");
        if(Func)
        {
            Func();
        }
#endif
        plugin *Plugin = LoadedPlugin.Plugin;
        if(Plugin->Run(Plugin, "__test", "some:data:thingy", 16))
        {
            printf("    Plugin->Run(..) success!\n");
        }
        else
        {
            printf("    Plugin->Run(..) failed!\n");
        }

#if 0
        UnloadPlugin(&LoadedPlugin);
#endif
    }

    return true;
}
