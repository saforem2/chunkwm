#ifndef CHUNKWM_OSX_CARBON_H
#define CHUNKWM_OSX_CARBON_H

#include <Carbon/Carbon.h>
#include "../../common/dispatch/carbon.h"

struct carbon_event_handler
{
    EventTargetRef EventTarget;
    EventHandlerUPP EventHandler;
    EventTypeSpec EventType[2];
    EventHandlerRef CurHandler;
};

bool BeginCarbonEventHandler(carbon_event_handler *Carbon);
bool EndCarbonEventHandler(carbon_event_handler *Carbon);

#endif
