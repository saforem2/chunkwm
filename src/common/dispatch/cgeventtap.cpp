#include "cgeventtap.h"

bool IsEventTapEnabled(event_tap *EventTap)
{
    bool Result = (EventTap->Handle && CGEventTapIsEnabled(EventTap->Handle));
    return Result;
}

bool BeginEventTap(event_tap *EventTap, eventtap_callback *Callback)
{
    EventTap->Handle = CGEventTapCreate(kCGSessionEventTap,
                                        kCGHeadInsertEventTap,
                                        kCGEventTapOptionDefault,
                                        EventTap->Mask,
                                        Callback,
                                        EventTap);

    if (IsEventTapEnabled(EventTap)) {
        EventTap->RunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault,
                                                                EventTap->Handle,
                                                                0);
        CFRunLoopAddSource(CFRunLoopGetMain(), EventTap->RunLoopSource, kCFRunLoopCommonModes);
        return true;
    } else {
        return false;
    }
}

void EndEventTap(event_tap *EventTap)
{
    if (IsEventTapEnabled(EventTap)) {
        CGEventTapEnable(EventTap->Handle, false);
        CFMachPortInvalidate(EventTap->Handle);
        CFRunLoopRemoveSource(CFRunLoopGetMain(), EventTap->RunLoopSource, kCFRunLoopCommonModes);
        CFRelease(EventTap->RunLoopSource);
        CFRelease(EventTap->Handle);
        EventTap->Handle = NULL;
    }
}
