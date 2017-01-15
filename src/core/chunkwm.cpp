#include <stdlib.h>
#include <stdio.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/event.h"

#include "plugin.h"

extern "C" void NSApplicationLoad();

int main(int Count, char **Args)
{
    NSApplicationLoad();

    carbon_event_handler Carbon = {};
    if(BeginCarbonEventHandler(&Carbon))
    {
        BeginPlugins();
        BeginSharedWorkspace();
        StartEventLoop();
        CFRunLoopRun();
    }

    return EXIT_SUCCESS;
}
