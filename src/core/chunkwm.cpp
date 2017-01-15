#include <stdlib.h>
#include <stdio.h>

#include "accessibility/carbon.h"
#include "accessibility/workspace.h"
#include "accessibility/event.h"

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
