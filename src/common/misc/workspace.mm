#include <Cocoa/Cocoa.h>
#include "workspace.h"
#include "assert.h"

char *WorkspaceCopyProcessNameAndPolicy(pid_t PID, uint32_t *ProcessPolicy)
{
    ASSERT(ProcessPolicy);

    char *ProcessName = NULL;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if (Application) {
        *ProcessPolicy = [Application activationPolicy];
        const char *ApplicationName = [[Application localizedName] UTF8String];
        if (ApplicationName) {
            ProcessName = strdup(ApplicationName);
        }
    }

    if (!ProcessName) {
        ProcessName = strdup("<unknown>");
    }

    return ProcessName;
}

char *WorkspaceCopyProcessName(pid_t PID)
{
    char *ProcessName = NULL;;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if (Application) {
        const char *ApplicationName = [[Application localizedName] UTF8String];
        if (ApplicationName) {
            ProcessName = strdup(ApplicationName);
        }
    }

    if (!ProcessName) {
        ProcessName = strdup("<unknown>");
    }

    return ProcessName;
}

application_launch_state WorkspaceGetApplicationLaunchState(pid_t PID)
{
    application_launch_state Result = Application_State_Failed;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];

    if (Application) {
        if ([Application isFinishedLaunching]) {
            Result = Application_State_Launched;
        } else {
            Result = Application_State_Launching;
        }
    }

    return Result;
}
