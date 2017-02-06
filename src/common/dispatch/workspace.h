#ifndef CHUNKWM_COMMON_WS_H
#define CHUNKWM_COMMON_WS_H

#include <unistd.h>

char *WorkspaceCopyProcessNameAndPolicy(pid_t PID, uint32_t *ProcessPolicy);
char *WorkspaceCopyProcessName(pid_t PID);

#endif
