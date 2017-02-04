#include <stdlib.h>
#include <stdio.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "callback.h"
#include "plugin.h"
#include "wqueue.h"

#include "../common/dispatch/carbon.cpp"
#include "../common/dispatch/workspace.mm"

#include "dispatch/carbon.cpp"
#include "dispatch/workspace.mm"
#include "dispatch/event.cpp"
#include "dispatch/display.cpp"

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

        StartEventLoop();
        CFRunLoopRun();
    }

    return EXIT_SUCCESS;
}
