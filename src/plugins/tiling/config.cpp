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
#include "../../common/misc/debug.h"

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

    while(*Message)
    {
        token ArgToken = GetToken(&Message);
        char *Arg = TokenToString(ArgToken);
        Args[(*Count)++] = Arg;
    }

#if 1
    for(int Index = 1;
        Index < *Count;
        ++Index)
    {
        DEBUG_PRINT("%d arg '%s'\n", Index, Args[Index]);
    }
#endif

    return Args;
}

inline void
FreeArguments(int Count, char **Args)
{
    for(int Index = 1;
        Index < Count;
        ++Index)
    {
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
    while(Command)
    {
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

typedef void (*command_func)(char *);
command_func WindowCommandDispatch(char Flag)
{
    switch(Flag)
    {
        case 'f': return FocusWindow;           break;
        case 's': return SwapWindow;            break;
        case 'i': return UseInsertionPoint;     break;
        case 't': return ToggleWindow;          break;
        case 'w': return WarpWindow;            break;
        case 'W': return WarpFloatingWindow;    break;
        case 'r': return TemporaryRatio;        break;
        case 'e': return AdjustWindowRatio;     break;
        case 'd': return SendWindowToDesktop;   break;
        case 'm': return SendWindowToMonitor;   break;

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
    const char *Short = "f:s:i:t:w:W:r:e:d:m:";

    struct option Long[] =
    {
        { "focus", required_argument, NULL, 'f' },
        { "swap", required_argument, NULL, 's' },
        { "use-insertion-point", required_argument, NULL, 'i' },
        { "toggle", required_argument, NULL, 't' },
        { "warp", required_argument, NULL, 'w' },
        { "warp-floating", required_argument, NULL, 'W' },
        { "use-temporary-ratio", required_argument, NULL, 'r' },
        { "adjust-window-edge", required_argument, NULL, 'e' },
        { "send-to-desktop", required_argument, NULL, 'd' },
        { "send-to-monitor", required_argument, NULL, 'm' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1)
    {
        switch(Option)
        {
            // NOTE(koekeishiya): The '-f', '-s', '-w' and '-e' flag support the same arguments.
            case 'f':
            case 's':
            case 'w':
            case 'e':
            {
                if((StringEquals(optarg, "west")) ||
                   (StringEquals(optarg, "east")) ||
                   (StringEquals(optarg, "north")) ||
                   (StringEquals(optarg, "south")) ||
                   (StringEquals(optarg, "prev")) ||
                   (StringEquals(optarg, "next")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'i':
            {
                if((StringEquals(optarg, "west")) ||
                   (StringEquals(optarg, "east")) ||
                   (StringEquals(optarg, "north")) ||
                   (StringEquals(optarg, "south")) ||
                   (StringEquals(optarg, "cancel")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'r':
            {
                float Float;
                if(sscanf(optarg, "%f", &Float) == 1)
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 't':
            {
                if((StringEquals(optarg, "float")) ||
                   (StringEquals(optarg, "split")) ||
                   (StringEquals(optarg, "fullscreen")) ||
                   (StringEquals(optarg, "native-fullscreen")) ||
                   (StringEquals(optarg, "parent")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'd':
            case 'm':
            {
                unsigned Unsigned;
                if((StringEquals(optarg, "prev")) ||
                   (StringEquals(optarg, "next")) ||
                   (sscanf(optarg, "%d", &Unsigned) == 1))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'W':
            {
                if((StringEquals(optarg, "fullscreen")) ||
                   (StringEquals(optarg, "left")) ||
                   (StringEquals(optarg, "right")) ||
                   (StringEquals(optarg, "top-left")) ||
                   (StringEquals(optarg, "top-right")) ||
                   (StringEquals(optarg, "bottom-left")) ||
                   (StringEquals(optarg, "bottom-right")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?':
            {
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
    switch(Flag)
    {
        case 'r': return RotateWindowTree;       break;
        case 'l': return ActivateSpaceLayout;    break;
        case 't': return ToggleSpace;            break;
        case 'm': return MirrorWindowTree;       break;
        case 'p': return AdjustSpacePadding;     break;
        case 'g': return AdjustSpaceGap;         break;
        case 'e': return EqualizeWindowTree;     break;
        case 's': return SerializeDesktop;       break;
        case 'd': return DeserializeDesktop;     break;

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
    const char *Short = "r:l:t:m:p:g:e";

    struct option Long[] =
    {
        { "rotate", required_argument, NULL, 'r' },
        { "layout", required_argument, NULL, 'l' },
        { "toggle", required_argument, NULL, 't' },
        { "mirror", required_argument, NULL, 'm' },
        { "padding", required_argument, NULL, 'p' },
        { "gap", required_argument, NULL, 'g' },
        { "equalize", no_argument, NULL, 'e' },
        { "serialize", required_argument, NULL, 's' },
        { "deserialize", required_argument, NULL, 'd' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1)
    {
        switch(Option)
        {
            case 'r':
            {
                if((StringEquals(optarg, "90")) ||
                   (StringEquals(optarg, "180")) ||
                   (StringEquals(optarg, "270")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'l':
            {
                if((StringEquals(optarg, "bsp")) ||
                   (StringEquals(optarg, "monocle")) ||
                   (StringEquals(optarg, "float")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 't':
            {
                if((StringEquals(optarg, "offset")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'm':
            {
                if((StringEquals(optarg, "vertical")) ||
                   (StringEquals(optarg, "horizontal")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'p':
            case 'g':
            {
                if((StringEquals(optarg, "inc")) ||
                   (StringEquals(optarg, "dec")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for desktop flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'e':
            {
                // NOTE(koekeishiya): This option takes no arguments
                command *Entry = ConstructCommand(Option, NULL);
                Command->Next = Entry;
                Command = Entry;
            } break;
            case 's':
            case 'd':
            {
                // NOTE(koekeishiya): This option takes a filepath as argument
                if(optarg)
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    missing selector for desktop flag '%c'\n", Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?':
            {
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
    switch(Flag)
    {
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
    while((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1)
    {
        switch(Option)
        {
            case 'f':
            {
                unsigned Unsigned;
                if((StringEquals(optarg, "prev")) ||
                   (StringEquals(optarg, "next")) ||
                   (sscanf(optarg, "%d", &Unsigned) == 1))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for monitor flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?':
            {
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

inline void
ParseQueryCommand(const char **Message, int SockFD)
{
    token Command = GetToken(Message);
    if(TokenEquals(Command, "window"))
    {
        token Selector = GetToken(Message);
        if(TokenEquals(Selector, "details"))
        {
            token ValueToken = GetToken(Message);
            if(ValueToken.Length > 0)
            {
                uint32_t WindowId;
                if(sscanf(ValueToken.Text, "%d", &WindowId) == 1)
                {
                    char *Response = QueryWindowDetails(WindowId);
                    if(Response)
                    {
                        WriteToSocket(Response, SockFD);
                        free(Response);
                        goto win_success;
                    }
                }
            }

            char Response[32];
            Response[0] = '\0';
            snprintf(Response, sizeof(Response), "%s", "invalid windowid");
            WriteToSocket(Response, SockFD);
win_success:;
        }
        else if(TokenEquals(Selector, "list"))
        {
            char *Response = QueryWindowsForActiveSpace();
            if(Response)
            {
                WriteToSocket(Response, SockFD);
                free(Response);
            }
        }
    }
}

inline bool
ParseRuleCommand(const char *Message, window_rule *Rule)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "o:n:e:s:d:";

    struct option Long[] =
    {
        { "owner", required_argument, NULL, 'o' },
        { "name", required_argument, NULL, 'n' },
        { "except", required_argument, NULL, 'e' },
        { "state", required_argument, NULL, 's' },
        { "desktop", required_argument, NULL, 'd' },
        { NULL, 0, NULL, 0 }
    };

    bool HasFilter = false;
    bool HasProperty = false;

    while((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1)
    {
        switch(Option)
        {
            case 'o':
            {
                Rule->Owner = strdup(optarg);
                HasFilter = true;
            } break;
            case 'n':
            {
                Rule->Name = strdup(optarg);
                HasFilter = true;
            } break;
            case 'e':
            {
                Rule->Except = strdup(optarg);
                HasFilter = true;
            } break;
            case 's':
            {
                Rule->State = strdup(optarg);
                HasProperty = true;
            } break;
            case 'd':
            {
                Rule->Desktop = strdup(optarg);
                HasProperty = true;
            } break;
            case '?':
            {
                Success = false;
                goto End;
            } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    if(!HasFilter)
    {
        fprintf(stderr, "chunkwm-tiling: window rule - no filter specified, ignored..\n");
        Success = false;
    }

    if(!HasProperty)
    {
        fprintf(stderr, "chunkwm-tiling: window rule - missing value for state, ignored..\n");
        Success = false;
    }

    FreeArguments(Count, Args);
    return Success;
}

void CommandCallback(int SockFD, const char *Type, const char *Message)
{
    printf("chunkwm-tiling recv: '%s %s'\n", Type, Message);

    if(StringEquals(Type, "query"))
    {
        ParseQueryCommand(&Message, SockFD);
    }
    else if(StringEquals(Type, "rule"))
    {
        window_rule Rule = {};
        if(ParseRuleCommand(Message, &Rule))
        {
            AddWindowRule(&Rule);
        }
    }
    else if(StringEquals(Type, "window"))
    {
        command Chain = {};
        bool Success = ParseWindowCommand(Message, &Chain);
        if(Success)
        {
            float Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                DEBUG_PRINT("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*WindowCommandDispatch(Command->Flag))(Command->Arg);
            }

            if(Ratio != CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO))
            {
                UpdateCVar(CVAR_BSP_SPLIT_RATIO, Ratio);
            }

            FreeCommandChain(&Chain);
        }
    }
    else if(StringEquals(Type, "desktop"))
    {
        command Chain = {};
        bool Success = ParseSpaceCommand(Message, &Chain);
        if(Success)
        {
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                DEBUG_PRINT("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*SpaceCommandDispatch(Command->Flag))(Command->Arg);
            }

            FreeCommandChain(&Chain);
        }
    }
    else if(StringEquals(Type, "monitor"))
    {
        command Chain = {};
        bool Success = ParseMonitorCommand(Message, &Chain);
        if(Success)
        {
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                DEBUG_PRINT("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*MonitorCommandDispatch(Command->Flag))(Command->Arg);
            }

            FreeCommandChain(&Chain);
        }
    }
    else
    {
        fprintf(stderr, "chunkwm-tiling: no match for '%s %s'\n", Type, Message);
    }
}
