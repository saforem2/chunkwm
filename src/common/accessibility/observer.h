#ifndef AXLIB_OBSERVER_H
#define AXLIB_OBSERVER_H

#include <Carbon/Carbon.h>

#define OBSERVER_CALLBACK(name) void name(AXObserverRef Observer, AXUIElementRef Element,\
                                          CFStringRef Notification, void *Reference)
typedef OBSERVER_CALLBACK(ObserverCallback);

struct ax_application;
struct ax_observer
{
    ax_application *Application;
    AXObserverRef Ref;
    bool Valid;
};

void AXLibConstructObserver(ax_application *Application, ObserverCallback Callback);
void AXLibDestroyObserver(ax_observer *Observer);

void AXLibStartObserver(ax_observer *Observer);
void AXLibStopObserver(ax_observer *Observer);

AXError AXLibAddObserverNotification(ax_observer *Observer, AXUIElementRef Ref, CFStringRef Notification, void *Reference);
void AXLibRemoveObserverNotification(ax_observer *Observer, AXUIElementRef Ref, CFStringRef Notification);

#endif
