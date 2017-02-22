#include <stdlib.h>
#include <stdio.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "hotloader.h"
#include "state.h"
#include "plugin.h"
#include "wqueue.h"

#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/misc/carbon.cpp"
#include "../common/misc/workspace.mm"

#include "../common/ipc/daemon.cpp"
#include "../common/config/cvar.cpp"

#include "dispatch/carbon.cpp"
#include "dispatch/workspace.mm"
#include "dispatch/event.cpp"
#include "dispatch/display.cpp"

#include "hotloader.cpp"
#include "state.cpp"
#include "callback.cpp"
#include "plugin.cpp"
#include "wqueue.cpp"
#include "config.cpp"

#define internal static
#define local_persist static

inline AXUIElementRef
SystemWideElement()
{
    local_persist AXUIElementRef Element;
    local_persist dispatch_once_t Token;

    dispatch_once(&Token, ^{
        Element = AXUIElementCreateSystemWide();
    });

    return Element;
}

#define CONFIG_FILE  "/.chunkwmrc"
#define CHUNKWM_PORT 3920

int main(int Count, char **Args)
{
    NSApplicationLoad();
    AXUIElementSetMessagingTimeout(SystemWideElement(), 1.0);

    carbon_event_handler Carbon = {};
    if(!BeginCarbonEventHandler(&Carbon))
    {
        fprintf(stderr, "chunkwm: failed to install carbon eventhandler! abort..\n");
        return EXIT_FAILURE;
    }

    if(!BeginCVars())
    {
        fprintf(stderr, "chunkwm: failed to initialize cvars! abort..\n");
        return EXIT_FAILURE;
    }

    if(!StartDaemon(CHUNKWM_PORT, DaemonCallback))
    {
        fprintf(stderr, "chunkwm: failed to initialize daemon! abort..\n");
        return EXIT_FAILURE;
    }

    const char *HomeEnv = getenv("HOME");
    if(!HomeEnv)
    {
        fprintf(stderr, "chunkwm: 'env HOME' not set! abort..\n");
        return EXIT_FAILURE;
    }

    unsigned HomeEnvLength = strlen(HomeEnv);
    unsigned ConfigFileLength = strlen(CONFIG_FILE);
    unsigned PathLength = HomeEnvLength + ConfigFileLength;

    char PathToConfigFile[PathLength + 1];
    PathToConfigFile[PathLength] = '\0';

    memcpy(PathToConfigFile, HomeEnv, HomeEnvLength);
    memcpy(PathToConfigFile + HomeEnvLength, CONFIG_FILE, ConfigFileLength);

    struct stat Buffer;
    if(stat(PathToConfigFile, &Buffer) != 0)
    {
        fprintf(stderr, "chunkwm: config '%s' not found!\n", PathToConfigFile);
        return EXIT_FAILURE;
    }

    if(!BeginPlugins())
    {
        fprintf(stderr, "chunkwm: failed to initialize critical mutex! abort..\n");
        return EXIT_FAILURE;
    }

    if(InitState())
    {
        BeginCallbackThreads(4);
        BeginSharedWorkspace();
        BeginDisplayHandler();

        // NOTE(koekeishiya): The config file is just an executable bash script!
        system(PathToConfigFile);

        // NOTE(koekeishiya): Read plugin directory from cvar.
        char *PluginDirectory = CVarStringValue("plugin_dir");
        if(PluginDirectory && CVarIntegerValue("hotload"))
        {
            HotloaderAddPath(PluginDirectory);
            HotloaderInit();
        }

        StartEventLoop();
        CFRunLoopRun();
    }
    else
    {
        fprintf(stderr, "chunkwm: failed to initialize critical mutex! abort..\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
