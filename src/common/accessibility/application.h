#ifndef AXLIB_APPLICATION_H
#define AXLIB_APPLICATION_H

#include <Carbon/Carbon.h>
#include <vector>

#include "observer.h"

struct macos_application
{
    AXUIElementRef Ref;
    macos_observer Observer;

    char *Name;
    pid_t PID;
    ProcessSerialNumber PSN;
};

macos_application *AXLibConstructFocusedApplication();
macos_application *AXLibConstructApplication(ProcessSerialNumber PSN, pid_t PID, char *Name);
void AXLibDestroyApplication(macos_application *Application);
bool AXLibAddApplicationObserver(macos_application *Application, ObserverCallback Callback);

std::vector<macos_application *> AXLibRunningProcesses(uint32_t ProcessFlags);

#endif
