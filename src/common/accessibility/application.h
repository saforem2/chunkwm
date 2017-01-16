#ifndef AXLIB_APPLICATION_H
#define AXLIB_APPLICATION_H

#include <Carbon/Carbon.h>
#include <unistd.h>
#include "observer.h"

struct ax_application
{
    AXUIElementRef Ref;
    ax_observer Observer;
    char *Name;
    pid_t PID;
    ProcessSerialNumber PSN;
    uint32_t Notifications;
};

ax_application *AXLibConstructApplication(pid_t PID, char *Name);
void AXLibDestroyApplication(ax_application *Application);
bool AXLibAddApplicationObserver(ax_application *Application, ObserverCallback Callback);

bool AXLibInitializeApplication(pid_t PID);
void AXLibInitializedApplication(ax_application *Application);

void AXLibAddApplicationWindows(ax_application *Application);
void AXLibRemoveApplicationWindows(ax_application *Application);

#endif
