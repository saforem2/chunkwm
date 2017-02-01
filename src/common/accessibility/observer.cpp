#include "observer.h"
#include "application.h"

void AXLibConstructObserver(macos_application *Application, ObserverCallback Callback)
{
    macos_observer *Observer = &Application->Observer;
    Observer->Enabled = false;

    AXError Result = AXObserverCreate(Application->PID, Callback, &Observer->Ref);
    Observer->Valid = (Result == kAXErrorSuccess);
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibStartObserver(macos_observer *Observer)
{
    if(!CFRunLoopContainsSource(CFRunLoopGetMain(),
                                AXObserverGetRunLoopSource(Observer->Ref),
                                kCFRunLoopDefaultMode))
    {
        Observer->Enabled = true;
        CFRunLoopAddSource(CFRunLoopGetMain(),
                           AXObserverGetRunLoopSource(Observer->Ref),
                           kCFRunLoopDefaultMode);
    }
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
AXError AXLibAddObserverNotification(macos_observer *Observer, AXUIElementRef Ref,
                                     CFStringRef Notification, void *Reference)
{
    return AXObserverAddNotification(Observer->Ref, Ref, Notification, Reference);
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibRemoveObserverNotification(macos_observer *Observer, AXUIElementRef Ref, CFStringRef Notification)
{
    AXObserverRemoveNotification(Observer->Ref, Ref, Notification);
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibStopObserver(macos_observer *Observer)
{
    Observer->Enabled = false;
    CFRunLoopSourceInvalidate(AXObserverGetRunLoopSource(Observer->Ref));
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibDestroyObserver(macos_observer *Observer)
{
    if(Observer->Enabled)
    {
        AXLibStopObserver(Observer);
    }

    CFRelease(Observer->Ref);
    Observer->Valid = false;
    Observer->Ref = NULL;
}
