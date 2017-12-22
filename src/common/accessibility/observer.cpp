#include "observer.h"
#include "application.h"
#include "../misc/assert.h"

/*
 * NOTE(koekeishiya): The following files must also be linked against:
 *
 * common/accessibility/application.cpp
 *
 */

/* NOTE(koekeishiya): Caller is responsible for calling 'AXLibDestroyObserver()'. */
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
    ASSERT(Observer && Observer->Ref);

    if (!CFRunLoopContainsSource(CFRunLoopGetMain(),
                                 AXObserverGetRunLoopSource(Observer->Ref),
                                 kCFRunLoopDefaultMode)) {
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
    ASSERT(Observer && Observer->Ref);
    ASSERT(Ref);
    ASSERT(Notification);

    return AXObserverAddNotification(Observer->Ref, Ref, Notification, Reference);
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibRemoveObserverNotification(macos_observer *Observer, AXUIElementRef Ref, CFStringRef Notification)
{
    ASSERT(Observer && Observer->Ref);
    ASSERT(Ref);

    AXObserverRemoveNotification(Observer->Ref, Ref, Notification);
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibStopObserver(macos_observer *Observer)
{
    ASSERT(Observer && Observer->Ref);

    Observer->Enabled = false;
    CFRunLoopSourceInvalidate(AXObserverGetRunLoopSource(Observer->Ref));
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid observer is passed! */
void AXLibDestroyObserver(macos_observer *Observer)
{
    ASSERT(Observer && Observer->Ref);

    if (Observer->Enabled) {
        AXLibStopObserver(Observer);
    }

    CFRelease(Observer->Ref);
    Observer->Valid = false;
    Observer->Ref = NULL;
}
