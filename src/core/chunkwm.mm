#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "hotloader.h"
#include "state.h"
#include "plugin.h"
#include "wqueue.h"
#include "cvar.h"

#include "constants.h"

#include "../common/misc/carbon.cpp"
#include "../common/misc/workspace.mm"

#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/ipc/daemon.cpp"
#include "../common/config/tokenize.cpp"
#include "../common/config/cvar.cpp"

#include "dispatch/carbon.cpp"
#include "dispatch/workspace.mm"
#include "dispatch/event.cpp"
#include "dispatch/display.cpp"

#include "hotloader.cpp"
#include "state.cpp"
#include "callback.cpp"
#include "plugin.cpp"
#include "wqueue.cpp"
#include "config.cpp"
#include "cvar.cpp"

#define internal static
#define local_persist static

internal char *ConfigAbsolutePath;

inline void
Fail(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
    exit(EXIT_FAILURE);
}

inline void
Warn(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
}

void SignalHandler(int Signal)
{
  void *Trace[15];
  size_t Size;

  Size = backtrace(Trace, 15);

  fprintf(stderr, "chunkwm: recv signal %d, printing stack trace\n", Signal);
  backtrace_symbols_fd(Trace, Size, STDERR_FILENO);
  exit(1);
}

inline AXUIElementRef
SystemWideElement()
{
    local_persist AXUIElementRef Element;
    local_persist dispatch_once_t Token;

    dispatch_once(&Token, ^{
        Element = AXUIElementCreateSystemWide();
    });

    return Element;
}

internal bool
ForkAndExec(char *Command)
{
    static const char *Shell = "/bin/bash";
    static const char *Arg   = "-c";

    int Pid = fork();
    if (Pid == 0) {
        char *Exec[] = { (char*)Shell, (char*)Arg, Command, NULL};
        int StatusCode = execvp(Exec[0], Exec);
        exit(StatusCode);
    }

    return true;
}

inline bool
CheckAccessibilityPrivileges()
{
    const void *Keys[] = { kAXTrustedCheckOptionPrompt };
    const void *Values[] = { kCFBooleanTrue };

    CFDictionaryRef Options = CFDictionaryCreate(kCFAllocatorDefault,
                                                 Keys,
                                                 Values,
                                                 sizeof(Keys) / sizeof(*Keys),
                                                 &kCFCopyStringDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);

    bool Result = AXIsProcessTrustedWithOptions(Options);
    CFRelease(Options);

    return Result;
}

inline void
SetConfigFile(char *ConfigFile, size_t Size)
{
    if (ConfigAbsolutePath) {
        snprintf(ConfigFile, Size, "%s", ConfigAbsolutePath);
    } else {
        const char *HomeEnv = getenv("HOME");
        if (!HomeEnv) {
            Fail("chunkwm: 'env HOME' not set! abort..\n");
        }

        snprintf(ConfigFile, Size, "%s/%s", HomeEnv, CHUNKWM_CONFIG);
    }

}

internal bool
ParseArguments(int Count, char **Args)
{
    int Option;
    const char *Short = "vc:";
    struct option Long[] = {
        { "version", no_argument, NULL, 'v' },
        { "config", required_argument, NULL, 'c' },
        { NULL, 0, NULL, 0 }
    };

    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        case 'v': {
            printf("chunkwm %d.%d.%d\n",
                    CHUNKWM_MAJOR,
                    CHUNKWM_MINOR,
                    CHUNKWM_PATCH);
            return true;
        } break;
        case 'c': {
            ConfigAbsolutePath = strdup(optarg);
            return false;
        } break;
        }
    }

    return false;
}

int main(int Count, char **Args)
{
    signal(SIGSEGV, SignalHandler);

    if (ParseArguments(Count, Args)) {
        return EXIT_SUCCESS;
    }

    if (!CheckAccessibilityPrivileges()) {
        Fail("chunkwm: could not access accessibility features! abort..\n");
    }

    if (!BeginCVars()) {
        Fail("chunkwm: failed to initialize cvars! abort..\n");
    }

    if (!StartDaemon(CHUNKWM_PORT, DaemonCallback)) {
        Fail("chunkwm: failed to initialize daemon! abort..\n");
    }

    char ConfigFile[MAX_LEN];
    ConfigFile[0] = '\0';
    SetConfigFile(ConfigFile, MAX_LEN);

    struct stat Buffer;
    if (stat(ConfigFile, &Buffer) != 0) {
        Fail("chunkwm: config '%s' not found!\n", ConfigFile);
    }

    if (!BeginPlugins()) {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    NSApplicationLoad();
    AXUIElementSetMessagingTimeout(SystemWideElement(), 1.0);

    carbon_event_handler Carbon = {};
    if (!BeginCarbonEventHandler(&Carbon)) {
        Fail("chunkwm: failed to install carbon eventhandler! abort..\n");
    }

    if (!InitState()) {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    if (!BeginDisplayHandler()) {
        Warn("chunkwm: could not register for display notifications..\n");
    }

    if (!BeginCallbackThreads(CHUNKWM_THREAD_COUNT)) {
        Warn("chunkwm: could not get semaphore, callback multi-threading disabled..\n");
    }

    BeginSharedWorkspace();

    // NOTE(koekeishiya): Read plugin directory from cvar.
    char *PluginDirectory = CVarStringValue(CVAR_PLUGIN_DIR);
    if (PluginDirectory && CVarIntegerValue(CVAR_PLUGIN_HOTLOAD)) {
        HotloaderAddPath(PluginDirectory);
        HotloaderInit();
    }

    // NOTE(koekeishiya): The config file is just an executable bash script!
    ForkAndExec(ConfigFile);

    if (!StartEventLoop()) {
        Fail("chunkwm: failed to start eventloop! abort..\n");
    }

    CFRunLoopRun();
    return EXIT_SUCCESS;
}
