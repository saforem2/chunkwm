#ifndef CHUNKWM_COMMON_WS_H
#define CHUNKWM_COMMON_WS_H

#include <Carbon/Carbon.h>

struct workspace_application_details
{
    char *ProcessName;
    ProcessSerialNumber PSN;
    pid_t PID;
};

#endif
