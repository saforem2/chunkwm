#define CHUNKWM_CORE

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/display.h"
#include "dispatch/event.h"

#include "hotload.h"
#include "hotloader.h"
#include "state.h"
#include "plugin.h"
#include "wqueue.h"
#include "cvar.h"
#include "constants.h"

#include "clog.h"
#include "clog.c"

#include "../common/misc/carbon.cpp"
#include "../common/misc/workspace.mm"

#include "../common/accessibility/display.mm"
#include "../common/accessibility/observer.cpp"
#include "../common/accessibility/application.cpp"
#include "../common/accessibility/window.cpp"
#include "../common/accessibility/element.cpp"

#include "../common/ipc/daemon.cpp"
#include "../common/config/tokenize.cpp"
#include "../common/config/cvar.cpp"

#include "sa_text.cpp"
#include "sa_core.cpp"
#include "sa_bundle.cpp"
#include "sa.mm"

#include "dispatch/carbon.cpp"
#include "dispatch/workspace.mm"
#include "dispatch/event.cpp"
#include "dispatch/display.cpp"

#include "hotload.c"
#include "hotloader.cpp"
#include "state.cpp"
#include "callback.cpp"
#include "plugin.cpp"
#include "wqueue.cpp"
#include "config.cpp"
#include "cvar.cpp"

#define internal static
#define local_persist static

#define SOCKET_PATH_FMT  "/tmp/chunkwm_%s-socket"
#define PIDFILE_PATH_FMT "/tmp/chunkwm_%s-pid"

internal carbon_event_handler Carbon;
internal hotloader Hotloader;
internal char *ConfigAbsolutePath;

internal inline void
Fail(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);
    exit(EXIT_FAILURE);
}

internal void
ForkExecWait(char *Command)
{
    static const char *Shell = "/bin/bash";
    static const char *Arg   = "-c";

    int Pid = fork();
    if (Pid == -1) {
        c_log(C_LOG_LEVEL_ERROR, "chunkwm: fork failed, config-file did not execute!\n");
    } else if (Pid > 0) {
        int Status;
        waitpid(Pid, &Status, 0);
    } else {
        char *Exec[] = { (char*)Shell, (char*)Arg, Command, NULL};
        int StatusCode = execvp(Exec[0], Exec);
        exit(StatusCode);
    }
}

internal inline void
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

internal void
ExecConfigFile()
{
    char ConfigFile[MAX_LEN];
    ConfigFile[0] = '\0';
    SetConfigFile(ConfigFile, MAX_LEN);

    struct stat Buffer;
    if (stat(ConfigFile, &Buffer) != 0) {
        Fail("chunkwm: config '%s' not found!\n", ConfigFile);
    }

    // NOTE(koekeishiya): The config file is just an executable bash script!
    ForkExecWait(ConfigFile);

    // NOTE(koekeishiya): Init hotloader for watching changes to plugins
    HotloadPlugins(&Hotloader, HotloadPluginCallback);
}

internal inline bool
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

internal inline bool
InitPidFileAndListener()
{
    char *User = getenv("USER");
    if (!User) {
        Fail("chunkwm: 'env USER' not set! abort..\n");
    }

    char PidPath[255];
    snprintf(PidPath, sizeof(PidPath), PIDFILE_PATH_FMT, User);

    pid_t Pid = getpid();
    int Handle = open(PidPath, O_CREAT | O_WRONLY, 0600);
    if (Handle == -1) {
        Fail("chunkwm: could not create pid-file! abort..\n");
    }

    if (flock(Handle, LOCK_EX | LOCK_NB) == -1) {
        Fail("chunkwm: could not lock pid-file! abort..\n");
    } else if (write(Handle, &Pid, sizeof(pid_t)) == -1) {
        Fail("chunkwm: could not write pid-file! abort..\n");
    }

    // NOTE(koekeishiya): we intentionally leave the handle open,
    // as calling close(..) will release the lock we just acquired.

    char SocketPath[255];
    snprintf(SocketPath, sizeof(SocketPath), SOCKET_PATH_FMT, User);

    return StartDaemon(SocketPath, DaemonCallback);
}

internal bool
ParseArguments(int Count, char **Args)
{
    int Option;
    const char *Short = "iulvc:";
    struct option Long[] = {
        { "version", no_argument, NULL, 'v' },
        { "config", required_argument, NULL, 'c' },
        { "install-sa", no_argument, NULL, 'i' },
        { "uninstall-sa", no_argument, NULL, 'u' },
        { "load-sa", no_argument, NULL, 'l' },
        { NULL, 0, NULL, 0 }
    };

    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        case 'i': {
            if (IsRoot()) {
                if (InstallSA()) {
                    printf("chunkwm: successfully installed sa!\n");
                } else {
                    printf("chunkwm: failed to install sa! make sure SIP is disabled.\n");
                }
            } else {
                printf("chunkwm: sudo privileges are required to install sa!\n");
            }
            return true;
        } break;
        case 'u': {
            if (IsRoot()) {
                int Result = UninstallSA();
                if (Result == -1) {
                    printf("chunkwm: failed to uninstall sa! make sure SIP is disabled.\n");
                } else if (Result == 0) {
                    printf("chunkwm: sa is not installed!\n");
                } else if (Result == 1) {
                    printf("chunkwm: successfully uninstalled sa! you can now reenable SIP.\n");
                }
            } else {
                printf("chunkwm: sudo privileges are required to uninstall sa!\n");
            }
            return true;
        } break;
        case 'l': {
            int Result = InjectSA();
            if (Result == -1) {
                printf("chunkwm: failed to load sa!\n");
            } else if (Result == 0) {
                printf("chunkwm: sa is not installed!\n");
            } else if (Result == 1) {
                printf("chunkwm: successfully loaded sa!\n");
            }
            return true;
        } break;
        case 'v': {
            printf("chunkwm %d.%d.%d (%s)\n",
                    CHUNKWM_MAJOR,
                    CHUNKWM_MINOR,
                    CHUNKWM_PATCH,
                    GIT_VERSION);
            return true;
        } break;
        case 'c': {
            ConfigAbsolutePath = strdup(optarg);
        } break;
        }
    }

    optind = 1;
    return false;
}

int main(int Count, char **Args)
{
    if (ParseArguments(Count, Args)) {
        return EXIT_SUCCESS;
    }

    if (IsRoot()) {
        Fail("chunkwm: running as root is not allowed! abort..\n");
    }

    if (!CheckAccessibilityPrivileges()) {
        Fail("chunkwm: could not access accessibility features! abort..\n");
    }

    if (!InitPidFileAndListener()) {
        Fail("chunkwm: failed to initialize daemon! abort..\n");
    }

    if (!BeginCVars()) {
        Fail("chunkwm: failed to initialize cvars! abort..\n");
    }

    if (!BeginPlugins()) {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    if (!BeginEventLoop()) {
        Fail("chunkwm: could not initialize event-loop! abort..\n");
    }

    if (!BeginCarbonEventHandler(&Carbon)) {
        Fail("chunkwm: failed to install carbon eventhandler! abort..\n");
    }

    if (!BeginDisplayHandler()) {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: could not register for display notifications..\n");
    }

    if (!BeginCallbackThreads(CHUNKWM_THREAD_COUNT)) {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: could not get semaphore, callback multi-threading disabled..\n");
    }

    if (!InitState()) {
        Fail("chunkwm: failed to initialize critical mutex! abort..\n");
    }

    if (!AXLibDisplayHasSeparateSpaces()) {
        Fail("chunkwm: displays have separate spaces is disabled! abort..\n");
    }

    BeginSharedWorkspace();
    StartEventLoop();
    ExecConfigFile();
    InjectSA();
    CFRunLoopRun();

    return EXIT_SUCCESS;
}
