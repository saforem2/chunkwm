#include "config.h"
#include "vspace.h"
#include "node.h"
#include "controller.h"
#include "constants.h"
#include "misc.h"

#include "../../common/ipc/daemon.h"
#include "../../common/config/cvar.h"
#include "../../common/config/tokenize.h"
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
                   (StringEquals(optarg, "south")))
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
                   (StringEquals(optarg, "focus")))
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
ParseConfigCommand(const char **Message)
{
    token Command = GetToken(Message);
    if(TokenEquals(Command, CVAR_SPACE_MODE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            DEBUG_PRINT("        value: '%.*s'\n", Value.Length, Value.Text);
            if(TokenEquals(Value, "bsp"))
            {
                UpdateCVar(Variable, Virtual_Space_Bsp);
            }
            else if(TokenEquals(Value, "monocle"))
            {
                UpdateCVar(Variable, Virtual_Space_Monocle);
            }
            else if(TokenEquals(Value, "float"))
            {
                UpdateCVar(Variable, Virtual_Space_Float);
            }
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_SPACE_OFFSET_TOP)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_BOTTOM)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_LEFT)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_RIGHT)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_GAP)) ||
            (TokenEquals(Command, CVAR_PADDING_STEP_SIZE)) ||
            (TokenEquals(Command, CVAR_GAP_STEP_SIZE)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            float FloatValue = TokenToFloat(Value);
            DEBUG_PRINT("        value: '%f'\n", FloatValue);
            UpdateCVar(Variable, FloatValue);
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_BSP_SPAWN_LEFT))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            int IntValue = TokenToInt(Value);
            DEBUG_PRINT("        value: '%d'\n", IntValue);
            UpdateCVar(Variable, IntValue);
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_BSP_OPTIMAL_RATIO)) ||
            (TokenEquals(Command, CVAR_BSP_SPLIT_RATIO)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            float FloatValue = TokenToFloat(Value);
            DEBUG_PRINT("        value: '%f'\n", FloatValue);
            UpdateCVar(Variable, FloatValue);
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_BSP_SPLIT_MODE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            DEBUG_PRINT("        value: '%.*s'\n", Value.Length, Value.Text);
            if(TokenEquals(Value, "optimal"))
            {
                UpdateCVar(Variable, Split_Optimal);
            }
            else if(TokenEquals(Value, "vertical"))
            {
                UpdateCVar(Variable, Split_Vertical);
            }
            else if(TokenEquals(Value, "horizontal"))
            {
                UpdateCVar(Variable, Split_Horizontal);
            }
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_WINDOW_FLOAT_TOPMOST)) ||
            (TokenEquals(Command, CVAR_WINDOW_FLOAT_NEXT)) ||
            (TokenEquals(Command, CVAR_WINDOW_FLOAT_CENTER)) ||
            (TokenEquals(Command, CVAR_MOUSE_FOLLOWS_FOCUS)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            int IntValue = TokenToInt(Value);
            DEBUG_PRINT("        value: '%d'\n", IntValue);
            UpdateCVar(Variable, IntValue);
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_WINDOW_FOCUS_CYCLE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        token Value = GetToken(Message);
        if(Value.Length > 0)
        {
            char *StringValue = TokenToString(Value);
            DEBUG_PRINT("        value: '%s'\n", StringValue);
            UpdateCVar(Variable, StringValue);
            free(StringValue);
        }
        else
        {
            fprintf(stderr, "        value: MISSING!!!\n");
        }

        free(Variable);
    }
    else
    {
        // NOTE(koekeishiya): The command we got is not a pre-defined string, but
        // we do allow custom options used for space-specific settings, etc...

        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        char Buffer[BUFFER_SIZE];
        int First;

        if(sscanf(Variable, "%d_%s", &First, Buffer) == 2)
        {
            if((StringEquals(Buffer, _CVAR_SPACE_OFFSET_TOP)) ||
                    (StringEquals(Buffer, _CVAR_SPACE_OFFSET_BOTTOM)) ||
                    (StringEquals(Buffer, _CVAR_SPACE_OFFSET_LEFT)) ||
                    (StringEquals(Buffer, _CVAR_SPACE_OFFSET_RIGHT)) ||
                    (StringEquals(Buffer, _CVAR_SPACE_OFFSET_GAP)))
            {
                token Value = GetToken(Message);
                if(Value.Length > 0)
                {
                    float FloatValue = TokenToFloat(Value);
                    DEBUG_PRINT("        value: '%f'\n", FloatValue);
                    UpdateCVar(Variable, FloatValue);
                }
                else
                {
                    fprintf(stderr, "        value: MISSING!!!\n");
                }
            }
            else if(StringEquals(Buffer, _CVAR_SPACE_MODE))
            {
                token Value = GetToken(Message);
                if(Value.Length > 0)
                {
                    DEBUG_PRINT("        value: '%.*s'\n", Value.Length, Value.Text);
                    if(TokenEquals(Value, "bsp"))
                    {
                        UpdateCVar(Variable, Virtual_Space_Bsp);
                    }
                    else if(TokenEquals(Value, "monocle"))
                    {
                        UpdateCVar(Variable, Virtual_Space_Monocle);
                    }
                    else if(TokenEquals(Value, "float"))
                    {
                        UpdateCVar(Variable, Virtual_Space_Float);
                    }
                }
                else
                {
                    fprintf(stderr, "        value: MISSING!!!\n");
                }
            }
            else
            {
                fprintf(stderr, " tiling daemon: '%s' is not a valid config option!\n", Variable);
            }
        }
        else
        {
            fprintf(stderr, " tiling daemon: '%.*s' is not a valid config option!\n", Command.Length, Command.Text);
        }
        free(Variable);
    }
}

inline void
FetchAndSendIntegerCVar(char *Variable, int SockFD)
{
    char Response[32];
    Response[0] = '\0';

    int Value = CVarIntegerValue(Variable);
    snprintf(Response, sizeof(Response), "%d", Value);

    WriteToSocket(Response, SockFD);
}

inline void
FetchAndSendFloatingPointCVar(char *Variable, int SockFD)
{
    char Response[32];
    Response[0] = '\0';

    float Value = CVarFloatingPointValue(Variable);
    snprintf(Response, sizeof(Response), "%f", Value);

    WriteToSocket(Response, SockFD);
}

inline void
FetchAndSendStringCVar(char *Variable, int SockFD)
{
    char *Value = CVarStringValue(Variable);
    WriteToSocket(Value, SockFD);
}

inline void
ParseQueryCommand(const char **Message, int SockFD)
{
    token Command = GetToken(Message);
    if(TokenEquals(Command, CVAR_SPACE_MODE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        char Response[32];
        Response[0] = '\0';

        int Value = CVarIntegerValue(Variable);
        switch(Value)
        {
            case Virtual_Space_Bsp:
            {
                snprintf(Response, sizeof(Response), "%s", "bsp");
            } break;
            case Virtual_Space_Monocle:
            {
                snprintf(Response, sizeof(Response), "%s", "monocle");
            } break;
            case Virtual_Space_Float:
            {
                snprintf(Response, sizeof(Response), "%s", "float");
            } break;
        }

        WriteToSocket(Response, SockFD);
        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_SPACE_OFFSET_TOP)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_BOTTOM)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_LEFT)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_RIGHT)) ||
            (TokenEquals(Command, CVAR_SPACE_OFFSET_GAP)) ||
            (TokenEquals(Command, CVAR_PADDING_STEP_SIZE)) ||
            (TokenEquals(Command, CVAR_GAP_STEP_SIZE)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendFloatingPointCVar(Variable, SockFD);

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_BSP_SPAWN_LEFT))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendIntegerCVar(Variable, SockFD);

        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_BSP_OPTIMAL_RATIO)) ||
            (TokenEquals(Command, CVAR_BSP_SPLIT_RATIO)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendFloatingPointCVar(Variable, SockFD);

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_BSP_SPLIT_MODE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        char Response[32];
        Response[0] = '\0';

        int Value = CVarIntegerValue(Variable);
        switch(Value)
        {
            case Split_Optimal:
            {
                snprintf(Response, sizeof(Response), "%s", "optimal");
            } break;
            case Split_Vertical:
            {
                snprintf(Response, sizeof(Response), "%s", "vertical");
            } break;
            case Split_Horizontal:
            {
                snprintf(Response, sizeof(Response), "%s", "horizontal");
            } break;
        }

        WriteToSocket(Response, SockFD);
        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_WINDOW_FLOAT_TOPMOST)) ||
            (TokenEquals(Command, CVAR_WINDOW_FLOAT_NEXT)) ||
            (TokenEquals(Command, CVAR_WINDOW_FLOAT_CENTER)) ||
            (TokenEquals(Command, CVAR_MOUSE_FOLLOWS_FOCUS)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendIntegerCVar(Variable, SockFD);

        free(Variable);
    }
    else if((TokenEquals(Command, CVAR_FOCUSED_WINDOW)) ||
            (TokenEquals(Command, CVAR_BSP_INSERTION_POINT)) ||
            (TokenEquals(Command, CVAR_ACTIVE_DESKTOP)) ||
            (TokenEquals(Command, CVAR_LAST_ACTIVE_DESKTOP)))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendIntegerCVar(Variable, SockFD);

        free(Variable);
    }
    else if(TokenEquals(Command, CVAR_WINDOW_FOCUS_CYCLE))
    {
        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        FetchAndSendStringCVar(Variable, SockFD);

        free(Variable);
    }
    else
    {
        // NOTE(koekeishiya): The command we got is not a pre-defined string, but
        // we do allow custom options used for space-specific settings, etc...

        char *Variable = TokenToString(Command);
        DEBUG_PRINT("        command: '%s'\n", Variable);

        char Buffer[BUFFER_SIZE];
        int First;

        if(sscanf(Variable, "%d_%s", &First, Buffer) == 2)
        {
            if((StringEquals(Buffer, _CVAR_SPACE_OFFSET_TOP)) ||
               (StringEquals(Buffer, _CVAR_SPACE_OFFSET_BOTTOM)) ||
               (StringEquals(Buffer, _CVAR_SPACE_OFFSET_LEFT)) ||
               (StringEquals(Buffer, _CVAR_SPACE_OFFSET_RIGHT)) ||
               (StringEquals(Buffer, _CVAR_SPACE_OFFSET_GAP)))
            {
                FetchAndSendFloatingPointCVar(Variable, SockFD);
            }
            else if(StringEquals(Buffer, _CVAR_SPACE_MODE))
            {
                char Response[32];
                Response[0] = '\0';

                int Value = CVarIntegerValue(Variable);
                switch(Value)
                {
                    case Virtual_Space_Bsp:
                    {
                        snprintf(Response, sizeof(Response), "%s", "bsp");
                    } break;
                    case Virtual_Space_Monocle:
                    {
                        snprintf(Response, sizeof(Response), "%s", "monocle");
                    } break;
                    case Virtual_Space_Float:
                    {
                        snprintf(Response, sizeof(Response), "%s", "float");
                    } break;
                }

                WriteToSocket(Response, SockFD);
            }
            else
            {
                fprintf(stderr, " tiling daemon: '%s' is not a valid config option!\n", Variable);
            }
        }
        else
        {
            fprintf(stderr, " tiling daemon: '%.*s' is not a valid config option!\n", Command.Length, Command.Text);
        }
        free(Variable);
    }
}

/* NOTE(koekeishiya): Parameters
 *
 * const char *Message
 * int SockFD
 *
 * */
DAEMON_CALLBACK(DaemonCallback)
{
    DEBUG_PRINT(" msg: '%s'\n", Message);
    token Type = GetToken(&Message);

    if(TokenEquals(Type, "config"))
    {
        ParseConfigCommand(&Message);
    }
    else if(TokenEquals(Type, "query"))
    {
        ParseQueryCommand(&Message, SockFD);
    }
    else if(TokenEquals(Type, "window"))
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
    else if(TokenEquals(Type, "desktop"))
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
    else if(TokenEquals(Type, "monitor"))
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
        fprintf(stderr, " tiling daemon: no match for '%.*s'\n", Type.Length, Type.Text);
    }
}
