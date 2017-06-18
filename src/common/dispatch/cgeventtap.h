#ifndef CHUNKWM_COMMON_EVENTTAP_H
#define CHUNKWM_COMMON_EVENTTAP_H

#include <Carbon/Carbon.h>

enum osx_event_mask
{
    Event_Mask_Alt = 0x00080000,
    Event_Mask_LAlt = 0x00000020,
    Event_Mask_RAlt = 0x00000040,

    Event_Mask_Shift = 0x00020000,
    Event_Mask_LShift = 0x00000002,
    Event_Mask_RShift = 0x00000004,

    Event_Mask_Cmd = 0x00100000,
    Event_Mask_LCmd = 0x00000008,
    Event_Mask_RCmd = 0x00000010,

    Event_Mask_Control = 0x00040000,
    Event_Mask_LControl = 0x00000001,
    Event_Mask_RControl = 0x00002000,

    Event_Mask_Fn = kCGEventFlagMaskSecondaryFn,
};

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
                    void *Reference)
typedef EVENTTAP_CALLBACK(eventtap_callback);

bool BeginEventTap(event_tap *EventTap, eventtap_callback *Callback);
void EndEventTap(event_tap *EventTap);
bool IsEventTapEnabled(event_tap *EventTap);

#endif
