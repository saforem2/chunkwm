#ifndef CHUNKWM_COMMON_CARBON_H
#define CHUNKWM_COMMON_CARBON_H

#include <Carbon/Carbon.h>

/*
 * NOTE(koekeishiya):
 * ProcessPolicy == 0     -> Appears in Dock, default for bundled applications.
 * ProcessPolicy == 1     -> Does not appear in Dock. Can create windows.
 *                           LSUIElement is set to 1.
 * ProcessPolicy == 2     -> Does not appear in Dock, cannot create windows.
 *                           LSBackgroundOnly is set to 1.
 */

#define PROCESS_POLICY_REGULAR 0
#define PROCESS_POLICY_LSUIELEMENT 1
#define PROCESS_POLICY_LSBACKGROUND_ONLY 2

enum carbon_process_policy
{
    // NOTE(koekeishiya): Appears in Dock, default for bundled applications.
    Process_Policy_Regular = (1 << 0),

    // NOTE(koekeishiya): Does not appear in Dock. Can create windows.
    Process_Policy_LSUIElement = (1 << 1),

    // NOTE(koekeishiya): Does not appear in Dock. Cannot create windows.
    Process_Policy_LSBackgroundOnly = (1 << 2),

    // NOTE(koekeishiya): Process is marked as a background only process.
    Process_Policy_CarbonBackgroundOnly = (1 << 3),
};

enum carbon_application_state
{
    Carbon_Application_State_None,
    Carbon_Application_State_In_Progress,
    Carbon_Application_State_Finished,
    Carbon_Application_State_Failed,
    Carbon_Application_State_Invalid,
};

struct carbon_application_details
{
    carbon_application_state volatile State;
    uint32_t volatile Attempts;
    char *ProcessName;
    uint32_t ProcessPolicy;
    bool ProcessBackground;
    ProcessSerialNumber PSN;
    pid_t PID;
};

carbon_application_details *BeginCarbonApplicationDetails(ProcessSerialNumber PSN);
void EndCarbonApplicationDetails(carbon_application_details *Info);

#endif
