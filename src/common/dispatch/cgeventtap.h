#ifndef CHUNKWM_OSX_CGEVENTTAP_H
#define CHUNKWM_OSX_CGEVENTTAP_H

#include <Carbon/Carbon.h>

struct event_tap
{
    CFMachPortRef Handle;
    CFRunLoopSourceRef RunLoopSource;
    CGEventMask Mask;
};

#define EVENTTAP_CALLBACK(name) \
    CGEventRef name(CGEventTapProxy Proxy, \
                    CGEventType Type, \
                    CGEventRef Event, \
                    void *Context)
typedef EVENTTAP_CALLBACK(eventtap_callback);

bool BeginEventTap(event_tap *EventTap, eventtap_callback *Callback);
void EndEventTap(event_tap *EventTap);
bool IsEventTapEnabled(event_tap *EventTap);

#endif
