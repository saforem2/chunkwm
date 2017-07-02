#ifndef CHUNKWM_CORE_PLUGIN_H
#define CHUNKWM_CORE_PLUGIN_H

#include "../api/plugin_api.h"
#include "../common/misc/string.h"

#include <map>

struct loaded_plugin
{
    char *Filename;
    void *Handle;
    plugin *Plugin;
    plugin_details *Info;
};

typedef std::map<plugin *, bool>  plugin_list;
typedef plugin_list::iterator plugin_list_iter;

bool BeginPlugins();

plugin_list *BeginPluginList(chunkwm_plugin_export Export);
void EndPluginList(chunkwm_plugin_export Export);

bool LoadPlugin(const char *Absolutepath, const char *Filename);
bool UnloadPlugin(const char *Absolutepath, const char *Filename);

typedef std::map<const char *, loaded_plugin *, string_comparator> loaded_plugin_list;
typedef loaded_plugin_list::iterator loaded_plugin_list_iter;

loaded_plugin_list *BeginLoadedPluginList();
void EndLoadedPluginList();

plugin *GetPluginFromFilename(const char *Filename);

#endif
