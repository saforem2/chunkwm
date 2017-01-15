#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <stdarg.h>

#include <string.h>
#include "plugin_api.h"

struct loaded_plugin
{
    void *Handle;
    plugin *Plugin;
};

void Error(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
    exit(EXIT_FAILURE);
}

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

bool VerifyPluginABI(plugin_details *Info)
{
    bool Result = (Info->ApiVersion == PLUGIN_API_VERSION);
    return Result;
}

void PrintPluginDetails(plugin_details *Info)
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

bool LoadPlugin(const char *File, loaded_plugin *LoadedPlugin)
{
    void *Handle = dlopen(File, RTLD_LAZY);
    if(Handle)
    {
        plugin_details *Info = (plugin_details *) dlsym(Handle, "Exports");
        if(Info)
        {
            if(!VerifyPluginABI(Info))
            {
                Error("Plugin '%s' ABI mismatch; expected %d, was %d\n",
                      Info->PluginName, PLUGIN_API_VERSION, Info->ApiVersion);
            }

            plugin *Plugin = Info->Initialize();
            LoadedPlugin->Handle = Handle;
            LoadedPlugin->Plugin = Plugin;
            PrintPluginDetails(Info);

            Plugin->Init(Plugin);
            return true;
        }
        else
        {
            dlclose(Handle);
        }
    }

    return false;
}

bool UnloadPlugin(loaded_plugin *LoadedPlugin)
{
    bool Result = false;
    if(LoadedPlugin->Handle)
    {
        plugin *Plugin = LoadedPlugin->Plugin;
        Plugin->DeInit(Plugin);
        Result = dlclose(LoadedPlugin->Handle) == 0;
    }

    return Result;
}

#if 0
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

int main(int Count, char **Args)
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

        UnloadPlugin(&LoadedPlugin);
    }

    return EXIT_SUCCESS;
}
