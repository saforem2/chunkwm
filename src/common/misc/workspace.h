#ifndef CHUNKWM_COMMON_WS_H
#define CHUNKWM_COMMON_WS_H

#include <unistd.h>

enum application_launch_state
{
    Application_State_Failed,
    Application_State_Launching,
    Application_State_Launched
};

char *WorkspaceCopyProcessNameAndPolicy(pid_t PID, uint32_t *ProcessPolicy);
char *WorkspaceCopyProcessName(pid_t PID);
application_launch_state WorkspaceGetApplicationLaunchState(pid_t PID);

#endif
