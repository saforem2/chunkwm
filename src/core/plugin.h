#ifndef CHUNKWM_CORE_PLUGIN_H
#define CHUNKWM_CORE_PLUGIN_H

#include "../api/plugin_api.h"

#include <map>

typedef std::map<plugin *, bool>  plugin_list;
typedef std::map<plugin *, bool>::iterator plugin_list_iter;

bool BeginPlugins();

plugin_list *BeginPluginList(chunkwm_plugin_export Export);
void EndPluginList(chunkwm_plugin_export Export);

bool LoadPlugin(const char *Absolutepath, const char *Filename);
bool UnloadPlugin(const char *Absolutepath, const char *Filename);

#endif
