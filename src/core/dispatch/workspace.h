#ifndef CHUNKWM_OSX_WS_H
#define CHUNKWM_OSX_WS_H

#include <Carbon/Carbon.h>

struct workspace_application_details
{
    char *ProcessName;
    ProcessSerialNumber PSN;
    pid_t PID;
};

void BeginSharedWorkspace();
void EndWorkspaceApplicationDetails(workspace_application_details *Info);
workspace_application_details *BeginWorkspaceApplicationDetails(char *ProcessName, ProcessSerialNumber PSN, pid_t PID);

#endif
