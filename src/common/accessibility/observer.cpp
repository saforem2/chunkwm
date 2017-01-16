#include "observer.h"
#include "application.h"

void AXLibConstructObserver(ax_application *Application, ObserverCallback Callback)
{
    AXError Result = AXObserverCreate(Application->PID, Callback, &Application->Observer.Ref);
    Application->Observer.Valid = (Result == kAXErrorSuccess);
    Application->Observer.Application = Application;
}

void AXLibStartObserver(ax_observer *Observer)
{
    if(!CFRunLoopContainsSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(Observer->Ref), kCFRunLoopDefaultMode))
        CFRunLoopAddSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(Observer->Ref), kCFRunLoopDefaultMode);
}

AXError AXLibAddObserverNotification(ax_observer *Observer, AXUIElementRef Ref, CFStringRef Notification, void *Reference)
{
    return AXObserverAddNotification(Observer->Ref, Ref, Notification, Reference);
}

void AXLibRemoveObserverNotification(ax_observer *Observer, AXUIElementRef Ref, CFStringRef Notification)
{
    AXObserverRemoveNotification(Observer->Ref, Ref, Notification);
}

void AXLibStopObserver(ax_observer *Observer)
{
    CFRunLoopSourceInvalidate(AXObserverGetRunLoopSource(Observer->Ref));
}

void AXLibDestroyObserver(ax_observer *Observer)
{
    if(Observer->Ref)
        CFRelease(Observer->Ref);

    Observer->Valid = false;
}
