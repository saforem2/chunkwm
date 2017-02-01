#ifndef AXLIB_OBSERVER_H
#define AXLIB_OBSERVER_H

#include <Carbon/Carbon.h>

#define OBSERVER_CALLBACK(name) void name(AXObserverRef Observer, AXUIElementRef Element,\
                                          CFStringRef Notification, void *Reference)
typedef OBSERVER_CALLBACK(ObserverCallback);

struct macos_application;
struct macos_observer
{
    AXObserverRef Ref;
    bool Enabled;
    bool Valid;
};

void AXLibConstructObserver(macos_application *Application, ObserverCallback Callback);
void AXLibDestroyObserver(macos_observer *Observer);

void AXLibStartObserver(macos_observer *Observer);
void AXLibStopObserver(macos_observer *Observer);

AXError AXLibAddObserverNotification(macos_observer *Observer, AXUIElementRef Ref, CFStringRef Notification, void *Reference);
void AXLibRemoveObserverNotification(macos_observer *Observer, AXUIElementRef Ref, CFStringRef Notification);

#endif
