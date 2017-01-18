#ifndef CHUNKWM_COMMON_CARBON_H
#define CHUNKWM_COMMON_CARBON_H

#include <Carbon/Carbon.h>

struct carbon_application_details
{
    char *ProcessName;
    uint32_t ProcessMode;
    ProcessSerialNumber PSN;
    pid_t PID;
};

#endif
