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

#include "constants.h"

#include "../common/misc/carbon.cpp"
#include "../common/misc/workspace.mm"

#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/ipc/daemon.cpp"
#include "../common/config/cvar.cpp"
#include "../common/config/tokenize.cpp"

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

inline bool
CheckAccessibilityPrivileges()
{
    const void *Keys[] = { kAXTrustedCheckOptionPrompt };
    const void *Values[] = { kCFBooleanTrue };

    CFDictionaryRef Options = CFDictionaryCreate(kCFAllocatorDefault,
                                                 Keys,
                                                 Values,
                                                 sizeof(Keys) / sizeof(*Keys),
                                                 &kCFCopyStringDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);

    bool Result = AXIsProcessTrustedWithOptions(Options);
    CFRelease(Options);

    return Result;
}

int main(int Count, char **Args)
{
    if(Count == 2)
    {
        if((strcmp(Args[1], "--version") == 0) ||
           (strcmp(Args[1], "-v") == 0))
        {
            printf("chunkwm %d.%d.%d\n",
                    CHUNKWM_MAJOR,
                    CHUNKWM_MINOR,
                    CHUNKWM_PATCH);
            return EXIT_SUCCESS;
        }
    }

    if(!CheckAccessibilityPrivileges())
    {
        fprintf(stderr, "chunkwm: could not access accessibility features! abort..\n");
        return EXIT_FAILURE;
    }

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

    char ConfigFile[MAX_LEN];
    ConfigFile[0] = '\0';

    snprintf(ConfigFile, sizeof(ConfigFile),
             "%s/%s", HomeEnv, CHUNKWM_CONFIG);

    struct stat Buffer;
    if(stat(ConfigFile, &Buffer) != 0)
    {
        fprintf(stderr, "chunkwm: config '%s' not found!\n", ConfigFile);
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
        system(ConfigFile);

        // NOTE(koekeishiya): Read plugin directory from cvar.
        char *PluginDirectory = CVarStringValue(CVAR_PLUGIN_DIR);
        if(PluginDirectory && CVarIntegerValue(CVAR_PLUGIN_HOTLOAD))
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
