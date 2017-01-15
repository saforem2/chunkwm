#ifndef CHUNKWM_OSX_WS_H
#define CHUNKWM_OSX_WS_H

#include <Carbon/Carbon.h>
#include <unistd.h>

struct workspace_application_details
{
    char *ProcessName;
    ProcessSerialNumber PSN;
    pid_t PID;
};

void BeginSharedWorkspace();
void EndWorkspaceApplicationDetails(workspace_application_details *Info);

#endif
