#ifndef CHUNKWM_COMMON_WS_H
#define CHUNKWM_COMMON_WS_H

#include <Carbon/Carbon.h>

struct workspace_application_details
{
    char *ProcessName;
    ProcessSerialNumber PSN;
    pid_t PID;
};

char *WorkspaceCopyProcessNameAndPolicy(pid_t PID, uint32_t *ProcessPolicy);
char *WorkspaceCopyProcessName(pid_t PID);

#endif
