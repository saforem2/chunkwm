#include "config.h"
#include "vspace.h"
#include "node.h"
#include "controller.h"
#include "rule.h"
#include "constants.h"
#include "misc.h"

#include "../../common/ipc/daemon.h"
#include "../../common/config/tokenize.h"
#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include <CoreFoundation/CFString.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define local_persist static

inline char **
BuildArguments(const char *Message, int *Count)
{
    char **Args = (char **) malloc(16 * sizeof(char *));
    *Count = 1;

    while (*Message) {
        token ArgToken = GetToken(&Message);
        char *Arg = TokenToString(ArgToken);
        Args[(*Count)++] = Arg;
    }

#if 0
    for (int Index = 1; Index < *Count; ++Index) {
        c_log(C_LOG_LEVEL_DEBUG, "%d arg '%s'\n", Index, Args[Index]);
    }
#endif

    return Args;
}

inline void
FreeArguments(int Count, char **Args)
{
    for (int Index = 1; Index < Count; ++Index) {
        free(Args[Index]);
    }
    free(Args);
}

struct command
{
    char Flag;
    char *Arg;
    struct command *Next;
};

inline void
FreeCommandChain(command *Chain)
{
    command *Command = Chain->Next;
    while (Command) {
        command *Next = Command->Next;
        free(Command->Arg);
        free(Command);
        Command = Next;
    }
}

inline command *
ConstructCommand(char Flag, char *Arg)
{
    command *Command = (command *) malloc(sizeof(command));

    Command->Flag = Flag;
    Command->Arg = Arg ? strdup(Arg) : NULL;
    Command->Next = NULL;

    return Command;
}

typedef void (*query_func)(char *, int);
typedef void (*command_func)(char *);
command_func WindowCommandDispatch(char Flag)
{
    switch (Flag) {
    case 'f': return FocusWindow;           break;
    case 's': return SwapWindow;            break;
    case 'i': return UseInsertionPoint;     break;
    case 't': return ToggleWindow;          break;
    case 'w': return WarpWindow;            break;
    case 'r': return TemporaryRatio;        break;
    case 'e': return AdjustWindowRatio;     break;
    case 'd': return SendWindowToDesktop;   break;
    case 'm': return SendWindowToMonitor;   break;
    case 'c': return CloseWindow;           break;
    case 'g': return GridLayout;            break;

    // NOTE(koekeishiya): silence compiler warning.
    default: return 0; break;
    }
}

inline bool
ParseWindowCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "f:s:i:t:w:r:e:d:m:cg:";

    struct option Long[] = {
        { "focus", required_argument, NULL, 'f' },
        { "swap", required_argument, NULL, 's' },
        { "use-insertion-point", required_argument, NULL, 'i' },
        { "toggle", required_argument, NULL, 't' },
        { "warp", required_argument, NULL, 'w' },
        { "use-temporary-ratio", required_argument, NULL, 'r' },
        { "adjust-window-edge", required_argument, NULL, 'e' },
        { "send-to-desktop", required_argument, NULL, 'd' },
        { "send-to-monitor", required_argument, NULL, 'm' },
        { "close", no_argument, NULL, 'c' },
        { "grid-layout", required_argument, NULL, 'g' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        // NOTE(koekeishiya): The '-f', '-s', '-w' flag support the same arguments.
        case 'f': {
            uint32_t WindowId;
            if ((StringEquals(optarg, "biggest")) ||
                (StringEquals(optarg, "west")) ||
                (StringEquals(optarg, "east")) ||
                (StringEquals(optarg, "north")) ||
                (StringEquals(optarg, "south")) ||
                (StringEquals(optarg, "prev")) ||
                (StringEquals(optarg, "next")) ||
                (sscanf(optarg, "%d", &WindowId) == 1)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 's':
        case 'w': {
            if ((StringEquals(optarg, "biggest")) ||
                (StringEquals(optarg, "west")) ||
                (StringEquals(optarg, "east")) ||
                (StringEquals(optarg, "north")) ||
                (StringEquals(optarg, "south")) ||
                (StringEquals(optarg, "prev")) ||
                (StringEquals(optarg, "next"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'e': {
            if ((StringEquals(optarg, "west")) ||
                (StringEquals(optarg, "east")) ||
                (StringEquals(optarg, "north")) ||
                (StringEquals(optarg, "south"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'i': {
            if ((StringEquals(optarg, "west")) ||
                (StringEquals(optarg, "east")) ||
                (StringEquals(optarg, "north")) ||
                (StringEquals(optarg, "south")) ||
                (StringEquals(optarg, "cancel"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'r': {
            float Float;
            if (sscanf(optarg, "%f", &Float) == 1) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 't': {
            if ((StringEquals(optarg, "float")) ||
                (StringEquals(optarg, "fade")) ||
                (StringEquals(optarg, "split")) ||
                (StringEquals(optarg, "sticky")) ||
                (StringEquals(optarg, "fullscreen")) ||
                (StringEquals(optarg, "native-fullscreen")) ||
                (StringEquals(optarg, "parent"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'd':
        case 'm': {
            unsigned Unsigned;
            if ((StringEquals(optarg, "prev")) ||
                (StringEquals(optarg, "next")) ||
                (sscanf(optarg, "%d", &Unsigned) == 1)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'c': {
                // NOTE(koekeishiya): This option takes no arguments
                command *Entry = ConstructCommand(Option, NULL);
                Command->Next = Entry;
                Command = Entry;
        } break;
        case 'g': {
            unsigned Unsigned;
            if ((sscanf(optarg, "%d:%d:%d:%d:%d:%d", &Unsigned, &Unsigned, &Unsigned, &Unsigned, &Unsigned, &Unsigned) == 6)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case '?': {
            Success = false;
            FreeCommandChain(Chain);
            goto End;
        } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

command_func SpaceCommandDispatch(char Flag)
{
    switch (Flag) {
    case 'r': return RotateWindowTree;       break;
    case 'l': return ActivateSpaceLayout;    break;
    case 't': return ToggleSpace;            break;
    case 'm': return MirrorWindowTree;       break;
    case 'p': return AdjustSpacePadding;     break;
    case 'g': return AdjustSpaceGap;         break;
    case 'e': return EqualizeWindowTree;     break;
    case 's': return SerializeDesktop;       break;
    case 'd': return DeserializeDesktop;     break;
    case 'f': return FocusDesktop;           break;
    case 'c': return CreateDesktop;          break;
    case 'a': return DestroyDesktop;         break;
    case 'M': return MoveDesktop;            break;

    // NOTE(koekeishiya): silence compiler warning.
    default: return 0; break;
    }
}

inline bool
ParseSpaceCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "r:l:t:m:p:g:ef:caM:";

    struct option Long[] = {
        { "rotate", required_argument, NULL, 'r' },
        { "layout", required_argument, NULL, 'l' },
        { "toggle", required_argument, NULL, 't' },
        { "mirror", required_argument, NULL, 'm' },
        { "padding", required_argument, NULL, 'p' },
        { "gap", required_argument, NULL, 'g' },
        { "equalize", no_argument, NULL, 'e' },
        { "serialize", required_argument, NULL, 's' },
        { "deserialize", required_argument, NULL, 'd' },
        { "focus", required_argument, NULL, 'f' },
        { "create", no_argument, NULL, 'c' },
        { "annihilate", no_argument, NULL, 'a' },
        { "move", required_argument, NULL, 'M' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        case 'f':
        case 'M': {
            unsigned Unsigned;
            if ((StringEquals(optarg, "prev")) ||
                (StringEquals(optarg, "next")) ||
                (sscanf(optarg, "%d", &Unsigned) == 1)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'r': {
            if ((StringEquals(optarg, "90")) ||
                (StringEquals(optarg, "180")) ||
                (StringEquals(optarg, "270"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'l': {
            if ((StringEquals(optarg, "bsp")) ||
                (StringEquals(optarg, "monocle")) ||
                (StringEquals(optarg, "float"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 't': {
            if ((StringEquals(optarg, "offset"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'm': {
            if ((StringEquals(optarg, "vertical")) ||
                (StringEquals(optarg, "horizontal"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'p':
        case 'g': {
            if ((StringEquals(optarg, "inc")) ||
                (StringEquals(optarg, "dec"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'c':
        case 'a':
        case 'e': {
            // NOTE(koekeishiya): These options take no arguments
            command *Entry = ConstructCommand(Option, NULL);
            Command->Next = Entry;
            Command = Entry;
        } break;
        case 's':
        case 'd': {
            // NOTE(koekeishiya): This option takes a filepath as argument
            if (optarg) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    missing selector for desktop flag '%c'\n", Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case '?': {
            Success = false;
            FreeCommandChain(Chain);
            goto End;
        } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

command_func MonitorCommandDispatch(char Flag)
{
    switch (Flag) {
    case 'f': return FocusMonitor; break;

    // NOTE(koekeishiya): silence compiler warning.
    default: return 0; break;
    }
}

inline bool
ParseMonitorCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "f:";

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1) {
        switch (Option) {
        case 'f': {
            unsigned Unsigned;
            if ((StringEquals(optarg, "prev")) ||
                (StringEquals(optarg, "next")) ||
                (sscanf(optarg, "%d", &Unsigned) == 1)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for monitor flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case '?': {
            Success = false;
            FreeCommandChain(Chain);
            goto End;
        } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

query_func QueryCommandDispatch(char Flag)
{
    switch (Flag) {
    case 'w': return QueryWindow;             break;
    case 'd': return QueryDesktop;            break;
    case 'm': return QueryMonitor;            break;
    case 'W': return QueryWindowsForDesktop;  break;
    case 'D': return QueryDesktopsForMonitor; break;
    case 'M': return QueryMonitorForDesktop;  break;

    // NOTE(koekeishiya): silence compiler warning.
    default: return 0; break;
    }
}
inline bool
ParseQueryCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "w:d:m:D:M:";

    struct option Long[] = {
        { "window", required_argument, NULL, 'w' },
        { "desktop", required_argument, NULL, 'd' },
        { "monitor", required_argument, NULL, 'm' },
        { "windows-for-desktop", required_argument, NULL, 'W' },
        { "desktops-for-monitor", required_argument, NULL, 'D' },
        { "monitor-for-desktop", required_argument, NULL, 'M' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        case 'w': {
            uint32_t WindowId;
            if ((StringEquals(optarg, "id")) ||
                (StringEquals(optarg, "owner")) ||
                (StringEquals(optarg, "name")) ||
                (StringEquals(optarg, "tag")) ||
                (StringEquals(optarg, "float")) ||
                (sscanf(optarg, "%d", &WindowId) == 1)) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'd': {
            if ((StringEquals(optarg, "id")) ||
                (StringEquals(optarg, "mode")) ||
                (StringEquals(optarg, "uuid")) ||
                (StringEquals(optarg, "windows")) ||
                (StringEquals(optarg, "monocle-index")) ||
                (StringEquals(optarg, "monocle-count"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'm': {
            if ((StringEquals(optarg, "id")) ||
                (StringEquals(optarg, "count"))) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for monitor flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case 'W':
        case 'D':
        case 'M': {
            int Integer;
            if (sscanf(optarg, "%d", &Integer) == 1) {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } else {
                c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for flag '%c'\n", optarg, Option);
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            }
        } break;
        case '?': {
            Success = false;
            FreeCommandChain(Chain);
            goto End;
        } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

inline bool
ParseRuleCommand(const char *Message, window_rule *Rule)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "o:n:r:R:e:s:d:Dl:a:g:";

    struct option Long[] = {
        { "owner", required_argument, NULL, 'o' },
        { "name", required_argument, NULL, 'n' },
        { "role", required_argument, NULL, 'r' },
        { "subrole", required_argument, NULL, 'R' },
        { "except", required_argument, NULL, 'e' },
        { "state", required_argument, NULL, 's' },
        { "desktop", required_argument, NULL, 'd' },
        { "monitor", required_argument, NULL, 'm' },
        { "follow-desktop", no_argument, NULL, 'D' },
        { "level", required_argument, NULL, 'l' },
        { "alpha", required_argument, NULL, 'a' },
        { "grid-layout", required_argument, NULL, 'g' },
        { NULL, 0, NULL, 0 }
    };

    bool HasFilter = false;
    bool HasProperty = false;

    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
        case 'o': {
            Rule->Owner = strdup(optarg);
            HasFilter = true;
        } break;
        case 'n': {
            Rule->Name = strdup(optarg);
            HasFilter = true;
        } break;
        case 'r': {
            Rule->Role = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingMacRoman);
            HasFilter = true;
        } break;
        case 'R': {
            Rule->Subrole = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingMacRoman);
            HasFilter = true;
        } break;
        case 'e': {
            Rule->Except = strdup(optarg);
            HasFilter = true;
        } break;
        case 's': {
            Rule->State = strdup(optarg);
            HasProperty = true;
        } break;
        case 'd': {
            Rule->Desktop = strdup(optarg);
            HasProperty = true;
        } break;
        case 'm': {
            Rule->Monitor = strdup(optarg);
            HasProperty = true;
        } break;
        case 'D': {
            Rule->FollowDesktop = true;
        } break;
        case 'l': {
            Rule->Level = strdup(optarg);
            HasProperty = true;
        } break;
        case 'a': {
            Rule->Alpha = strdup(optarg);
            HasProperty = true;
        } break;
        case 'g': {
            Rule->GridLayout = strdup(optarg);
            HasProperty = true;
        } break;
        case '?': {
            Success = false;
            goto End;
        } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    if (!HasFilter) {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-tiling: window rule - no filter specified, ignored..\n");
        Success = false;
    }

    if (!HasProperty) {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-tiling: window rule - missing value for state, ignored..\n");
        Success = false;
    }

    FreeArguments(Count, Args);
    return Success;
}

void CommandCallback(int SockFD, const char *Type, const char *Message)
{
    if (StringEquals(Type, "query")) {
        command Chain = {};
        bool Success = ParseQueryCommand(Message, &Chain);
        if (Success) {
            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_DEBUG, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*QueryCommandDispatch(Command->Flag))(Command->Arg, SockFD);
            }

            FreeCommandChain(&Chain);
        }
    } else if (StringEquals(Type, "rule")) {
        window_rule Rule = {};
        if (ParseRuleCommand(Message, &Rule)) {
            AddWindowRule(&Rule);
        }
    } else if (StringEquals(Type, "window")) {
        command Chain = {};
        bool Success = ParseWindowCommand(Message, &Chain);
        if (Success) {
            float Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_DEBUG, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*WindowCommandDispatch(Command->Flag))(Command->Arg);
            }

            if (Ratio != CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO)) {
                UpdateCVar(CVAR_BSP_SPLIT_RATIO, Ratio);
            }

            FreeCommandChain(&Chain);
        }
    } else if (StringEquals(Type, "desktop")) {
        command Chain = {};
        bool Success = ParseSpaceCommand(Message, &Chain);
        if (Success) {
            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_DEBUG, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*SpaceCommandDispatch(Command->Flag))(Command->Arg);
            }

            FreeCommandChain(&Chain);
        }
    } else if (StringEquals(Type, "monitor")) {
        command Chain = {};
        bool Success = ParseMonitorCommand(Message, &Chain);
        if (Success) {
            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_DEBUG, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*MonitorCommandDispatch(Command->Flag))(Command->Arg);
            }

            FreeCommandChain(&Chain);
        }
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-tiling: no match for '%s %s'\n", Type, Message);
    }
}
