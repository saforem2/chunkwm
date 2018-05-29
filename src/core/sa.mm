#import <ScriptingBridge/ScriptingBridge.h>
#include <string.h>

static inline bool IsSIPEnabled(void)
{
    // TODO(koekeishiya): Is there an API for this or do we need to parse the output of 'csrutil status' ?..
}

static bool InstallSA(void)
{
    if (IsSIPEnabled()) {
        return false;
    }

    // TODO(koekeishiya): Figure out a path where we place the bundle (at compile/install time)..
    // NOTE(koekeishiya): We just call cp using system to do the copy for now..
    // system("cp -Rf ./CHWMInjector.osax /System/Library/ScriptingAdditions 2>/dev/null");
    // return true;
    return false;
}

static bool UninstallSA(void)
{
    if (IsSIPEnabled()) {
        return false;
    }

    // NOTE(koekeishiya): We just call cp using system to do removal for now..
    system("rm -rf /System/Library/ScriptingAdditions/CHWMInjector.osax 2>/dev/null");
    return true;
}

static bool InjectSA(void)
{
    for (NSRunningApplication *Application in [[NSWorkspace sharedWorkspace] runningApplications]) {
        pid_t PID = Application.processIdentifier;
        const char *Name = [[Application localizedName] UTF8String];
        if (Name && strcmp(Name, "Dock") == 0) {
            SBApplication *SBApp = [SBApplication applicationWithProcessIdentifier:PID];
            [SBApp setTimeout:10*60];
            [SBApp setSendMode:kAEWaitReply];
            [SBApp sendEvent:'ascr' id:'gdut' parameters:0];
            [SBApp setSendMode:kAENoReply];
            [SBApp sendEvent:'CHWM' id:'injc' parameters:0];
            return true;
        }
    }
    return false;
}
