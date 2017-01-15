#ifndef CHUNKWM_COMMON_WS_H
#define CHUNKWM_COMMON_WS_H

#include <unistd.h>

void SharedWorkspaceActivateApplication(pid_t PID);
bool SharedWorkspaceIsApplicationActive(pid_t PID);
bool SharedWorkspaceIsApplicationHidden(pid_t PID);

#endif
