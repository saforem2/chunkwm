#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "hotloader.h"
#include "state.h"
#include "plugin.h"
#include "wqueue.h"
#include "cvar.h"

#include "constants.h"

#include "../common/misc/carbon.cpp"
#include "../common/misc/workspace.mm"

#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/ipc/daemon.cpp"
#include "../common/config/tokenize.cpp"
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
#include "cvar.cpp"

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

inline bool
CheckArguments(int Count, char **Args)
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
            return true;
        }
    }

    return false;
}

inline void
Fail(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
    exit(EXIT_FAILURE);
}

inline void
Warn(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
}

int main(int Count, char **Args)
{
    if(CheckArguments(Count, Args))
    {
        return EXIT_SUCCESS;
    }

    if(!CheckAccessibilityPrivileges())
    {
        Fail("chunkwm: could not access accessibility features! abort..\n");
    }

    if(!BeginCVars())
    {
        Fail("chunkwm: failed to initialize cvars! abort..\n");
    }

    if(!StartDaemon(CHUNKWM_PORT, DaemonCallback))
    {
        Fail("chunkwm: failed to initialize daemon! abort..\n");
    }

    const char *HomeEnv = getenv("HOME");
    if(!HomeEnv)
    {
        Fail("chunkwm: 'env HOME' not set! abort..\n");
    }

    char ConfigFile[MAX_LEN];
    ConfigFile[0] = '\0';

    snprintf(ConfigFile, sizeof(ConfigFile),
             "%s/%s", HomeEnv, CHUNKWM_CONFIG);

    struct stat Buffer;
    if(stat(ConfigFile, &Buffer) != 0)
    {
        Fail("chunkwm: config '%s' not found!\n", ConfigFile);
    }

    if(!BeginPlugins())
    {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    NSApplicationLoad();
    AXUIElementSetMessagingTimeout(SystemWideElement(), 1.0);

    carbon_event_handler Carbon = {};
    if(!BeginCarbonEventHandler(&Carbon))
    {
        Fail("chunkwm: failed to install carbon eventhandler! abort..\n");
    }

    if(!InitState())
    {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    if(!BeginDisplayHandler())
    {
        Warn("chunkwm: could not register for display notifications..\n");
    }

    if(!BeginCallbackThreads(CHUNKWM_THREAD_COUNT))
    {
        Warn("chunkwm: could not get semaphore, callback multi-threading disabled..\n");
    }

    BeginSharedWorkspace();

    // NOTE(koekeishiya): The config file is just an executable bash script!
    system(ConfigFile);

    // NOTE(koekeishiya): Read plugin directory from cvar.
    char *PluginDirectory = CVarStringValue(CVAR_PLUGIN_DIR);
    if(PluginDirectory && CVarIntegerValue(CVAR_PLUGIN_HOTLOAD))
    {
        HotloaderAddPath(PluginDirectory);
        HotloaderInit();
    }

    if(!StartEventLoop())
    {
        Fail("chunkwm: failed to start eventloop! abort..\n");
    }

    CFRunLoopRun();
    return EXIT_SUCCESS;
}
