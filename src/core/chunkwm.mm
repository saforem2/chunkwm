#include <stdlib.h>
#include <stdio.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "hotloader.h"
#include "state.h"
#include "callback.h"
#include "plugin.h"
#include "wqueue.h"

#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/dispatch/carbon.cpp"
#include "../common/dispatch/workspace.mm"

#include "dispatch/carbon.cpp"
#include "dispatch/workspace.mm"
#include "dispatch/event.cpp"
#include "dispatch/display.cpp"

#include "hotloader.cpp"
#include "state.cpp"
#include "callback.cpp"
#include "plugin.cpp"
#include "wqueue.cpp"

#define internal static
#define local_persist static

internal inline AXUIElementRef
SystemWideElement()
{
    local_persist AXUIElementRef Element;
    local_persist dispatch_once_t Token;

    dispatch_once(&Token, ^{
        Element = AXUIElementCreateSystemWide();
    });

    return Element;
}

internal const char *PluginDirectory = "/Users/Koe/Documents/programming/C++/chunkwm/plugins";

int main(int Count, char **Args)
{
    NSApplicationLoad();
    AXUIElementSetMessagingTimeout(SystemWideElement(), 1.0);

    carbon_event_handler Carbon = {};
    if(BeginCarbonEventHandler(&Carbon))
    {
        BeginCallbackThreads(4);
        BeginSharedWorkspace();
        BeginDisplayHandler();

        BeginPlugins();

        if(InitState())
        {
            // TODO(koekeishiya): Read plugin directory from config or something.
            HotloaderAddPath(PluginDirectory);
            HotloaderInit();

            StartEventLoop();
            CFRunLoopRun();
        }
        else
        {
            fprintf(stderr, "chunkwm: failed to initialize critical mutex! abort..\n");
        }
    }
    else
    {
        fprintf(stderr, "chunkwm: failed to install carbon eventhandler! abort..\n");
    }

    return EXIT_SUCCESS;
}
