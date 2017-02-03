#include <Cocoa/Cocoa.h>

char *WorkspaceCopyProcessNameAndPolicy(pid_t PID, uint32_t *ProcessPolicy)
{
    char *ProcessName = NULL;;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if(Application)
    {
        *ProcessPolicy = [Application activationPolicy];
        const char *ApplicationName = [[Application localizedName] UTF8String];
        if(ApplicationName)
        {
            ProcessName = strdup(ApplicationName);
        }
    }

    if(!ProcessName)
    {
        ProcessName = strdup("<Unknown>");
    }

    return ProcessName;
}

char *WorkspaceCopyProcessName(pid_t PID)
{
    char *ProcessName = NULL;;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if(Application)
    {
        const char *ApplicationName = [[Application localizedName] UTF8String];
        if(ApplicationName)
        {
            ProcessName = strdup(ApplicationName);
        }
    }

    if(!ProcessName)
    {
        ProcessName = strdup("<Unknown>");
    }

    return ProcessName;
}
