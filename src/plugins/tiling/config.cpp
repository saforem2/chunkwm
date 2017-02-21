#include "config.h"
#include "vspace.h"
#include "node.h"
#include "controller.h"
#include "constants.h"
#include "misc.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define local_persist static

struct token
{
    const char *Text;
    unsigned Length;
};

inline bool
TokenEquals(token Token, const char *Match)
{
    const char *At = Match;
    for(int Index = 0;
        Index < Token.Length;
        ++Index, ++At)
    {
        if((*At == 0) || (Token.Text[Index] != *At))
        {
            return false;
        }
    }

    bool Result = (*At == 0);
    return Result;
}

token GetToken(const char **Data)
{
    token Token;

    Token.Text = *Data;
    while(**Data && **Data != ' ')
    {
        ++(*Data);
    }

    Token.Length = *Data - Token.Text;
    ASSERT(**Data == ' ' || **Data == '\0');

    if(**Data == ' ')
    {
        ++(*Data);
    }
    else
    {
        // NOTE(koekeishiya): Do not go past the null-terminator!
    }

    return Token;
}

inline char *
TokenToString(token Token)
{
    char *Result = (char *) malloc(Token.Length + 1);
    Result[Token.Length] = '\0';
    memcpy(Result, Token.Text, Token.Length);
    return Result;
}

inline float
TokenToFloat(token Token)
{
    float Result = 0;
    char *String = TokenToString(Token);
    sscanf(String, "%f", &Result);
    free(String);
    return Result;
}

inline int
TokenToInt(token Token)
{
    int Result = 0;
    char *String = TokenToString(Token);
    sscanf(String, "%d", &Result);
    free(String);
    return Result;
}

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

#if 0
    for(int Index = 1;
        Index < *Count;
        ++Index)
    {
        printf("%d arg '%s'\n", Index, Args[Index]);
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
    Command->Arg = strdup(Arg);
    Command->Next = NULL;

    return Command;
}

inline bool
ParseWindowCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "f:s:i:";

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1)
    {
        switch(Option)
        {
            // NOTE(koekeishiya): Both the '-f', '-s' and '-i' flag support the same arguments.
            case 'f':
            case 's':
            case 'i':
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

/* NOTE(koekeishiya): Parameters
 *
 * const char *Message
 * int SockFD
 *
 * */
DAEMON_CALLBACK(DaemonCallback)
{
    token Type = GetToken(&Message);

    if(TokenEquals(Type, "config"))
    {
        token Command = GetToken(&Message);
        if(TokenEquals(Command, CVAR_SPACE_MODE))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                printf("        value: '%.*s'\n", Value.Length, Value.Text);
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
                (TokenEquals(Command, CVAR_SPACE_OFFSET_GAP)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                float FloatValue = TokenToFloat(Value);
                printf("        value: '%f'\n", FloatValue);
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
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                int IntValue = TokenToInt(Value);
                printf("        value: '%d'\n", IntValue);
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
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                float FloatValue = TokenToFloat(Value);
                printf("        value: '%f'\n", FloatValue);
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
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                printf("        value: '%.*s'\n", Value.Length, Value.Text);
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
                (TokenEquals(Command, CVAR_MOUSE_FOLLOWS_FOCUS)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                int IntValue = TokenToInt(Value);
                printf("        value: '%d'\n", IntValue);
                UpdateCVar(Variable, IntValue);
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
            printf("        command: '%s'\n", Variable);

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
                    token Value = GetToken(&Message);
                    if(Value.Length > 0)
                    {
                        float FloatValue = TokenToFloat(Value);
                        printf("        value: '%f'\n", FloatValue);
                        UpdateCVar(Variable, FloatValue);
                    }
                    else
                    {
                        fprintf(stderr, "        value: MISSING!!!\n");
                    }
                }
                else if(StringEquals(Buffer, _CVAR_SPACE_MODE))
                {
                    token Value = GetToken(&Message);
                    if(Value.Length > 0)
                    {
                        printf("        value: '%.*s'\n", Value.Length, Value.Text);
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
    else if(TokenEquals(Type, "window"))
    {
        command Chain = {};
        bool Success = ParseWindowCommand(Message, &Chain);
        if(Success)
        {
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                printf("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);

                /* NOTE(koekeishiya): flags description:
                 * f: focus
                 * s: swap
                 * m: move (detach and reinsert)
                 * i: insertion point (previously 'mark' window)
                 * z: zoom
                 * */

                // TODO(koekeishiya): Replace if-branches with jump-table
                if(Command->Flag == 'f')
                {
                    FocusWindow(Command->Arg);
                }
                else if(Command->Flag == 's')
                {
                    SwapWindow(Command->Arg);
                }
                else if(Command->Flag == 'i')
                {
                    UseInsertionPoint(Command->Arg);
                }
            }

            FreeCommandChain(&Chain);
        }
    }
    else
    {
        fprintf(stderr, " tiling daemon: no match for '%.*s'\n", Type.Length, Type.Text);
    }
}
