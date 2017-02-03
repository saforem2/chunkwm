#ifndef CHUNKWM_COMMON_CARBON_H
#define CHUNKWM_COMMON_CARBON_H

#include <Carbon/Carbon.h>

struct carbon_application_details
{
    char *ProcessName;
    uint32_t ProcessPolicy;
    bool ProcessBackground;
    ProcessSerialNumber PSN;
    pid_t PID;
};

carbon_application_details *BeginCarbonApplicationDetails(ProcessSerialNumber PSN);
void EndCarbonApplicationDetails(carbon_application_details *Info);

#endif
