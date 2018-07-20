#include "hotloader.h"
#include "hotload.h"

#include "../common/ipc/daemon.h"

#include "constants.h"
#include "cvar.h"
#include "clog.h"

#include <sys/stat.h>
#include <string.h>

#define internal static

internal void
PerformIOOperation(const char *Op, char *Filename)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, CHUNKWM_PORT)) {
        char Message[256];
        Message[0] = '\0';
        snprintf(Message, sizeof(Message), "%s %s", Op, Filename);
        WriteToSocket(Message, SockFD);
        CloseSocket(SockFD);
    }
}

internal HOTLOADER_CALLBACK(HotloadPluginCallback)
{
    c_log(C_LOG_LEVEL_DEBUG, "hotloader: plugin '%s' changed!\n", filename);

    c_log(C_LOG_LEVEL_DEBUG, "hotloader: unloading plugin '%s'\n", filename);
    PerformIOOperation("core::unload", filename);

    struct stat Buffer;
    if (stat(absolutepath, &Buffer) == 0) {
        c_log(C_LOG_LEVEL_DEBUG, "hotloader: loading plugin '%s'\n", filename);
        PerformIOOperation("core::load", filename);
    }
}

void HotloadPlugins(hotloader *Hotloader, hotloader_callback Callback)
{
    char *PluginDirectory = CVarStringValue(CVAR_PLUGIN_DIR);
    if (PluginDirectory && CVarIntegerValue(CVAR_PLUGIN_HOTLOAD)) {
        if (hotloader_add_catalog(Hotloader, PluginDirectory, ".so") &&
            hotloader_begin(Hotloader, Callback)) {
            c_log(C_LOG_LEVEL_DEBUG, "chunkwm: watching '%s' for changes to plugins!\n", PluginDirectory);
        } else {
            c_log(C_LOG_LEVEL_WARN, "chunkwm: could not watch directory '%s' for changes to plugins!\n", PluginDirectory);
        }
    }
}
