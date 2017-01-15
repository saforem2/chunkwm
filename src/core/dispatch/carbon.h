#ifndef CHUNKWM_OSX_CARBON_H
#define CHUNKWM_OSX_CARBON_H

#include <Carbon/Carbon.h>
#include <unistd.h>

struct carbon_event_handler
{
    EventTargetRef EventTarget;
    EventHandlerUPP EventHandler;
    EventTypeSpec EventType[2];
    EventHandlerRef CurHandler;
};

struct carbon_application_details
{
    char *ProcessName;
    uint32_t ProcessMode;
    ProcessSerialNumber PSN;
    pid_t PID;
};

bool BeginCarbonEventHandler(carbon_event_handler *Carbon);
bool EndCarbonEventHandler(carbon_event_handler *Carbon);
void EndCarbonApplicationDetails(carbon_application_details *Info);

#endif
