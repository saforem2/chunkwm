#include "config.h"
#include "vspace.h"
#include "node.h"
#include "constants.h"

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

    for(int Index = 1;
        Index < *Count;
        ++Index)
    {
        printf("%d arg '%s'\n", Index, Args[Index]);
    }

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

inline bool
ParseWindowCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "f:s:";

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1)
    {
        switch(Option)
        {
            case 'f':
            case 's':
            {
                printf("    %c: '%s'\n", Option, optarg);

                Command->Next = (command *) malloc(sizeof(command));
                Command = Command->Next;

                Command->Flag = Option;
                Command->Arg = strdup(optarg);
                Command->Next = NULL;
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
            float FloatValue = TokenToFloat(Value);
            printf("        value: '%f'\n", FloatValue);

            UpdateCVar(Variable, FloatValue);
            free(Variable);
        }
        else if(TokenEquals(Command, CVAR_BSP_SPAWN_LEFT))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            int IntValue = TokenToInt(Value);
            printf("        value: '%d'\n", IntValue);

            UpdateCVar(Variable, IntValue);
            free(Variable);
        }
        else if((TokenEquals(Command, CVAR_BSP_OPTIMAL_RATIO)) ||
                (TokenEquals(Command, CVAR_BSP_SPLIT_RATIO)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            float FloatValue = TokenToFloat(Value);
            printf("        value: '%f'\n", FloatValue);

            UpdateCVar(Variable, FloatValue);
            free(Variable);
        }
        else if(TokenEquals(Command, CVAR_BSP_SPLIT_MODE))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
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
            free(Variable);
        }
        else if(TokenEquals(Command, CVAR_WINDOW_FLOAT_TOPMOST))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            int IntValue = TokenToInt(Value);
            printf("        value: '%d'\n", IntValue);

            UpdateCVar(Variable, IntValue);
            free(Variable);
        }
        else
        {
            printf(" tiling daemon: '%.*s' is not a valid config option!\n", Command.Length, Command.Text);
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
            }

            FreeCommandChain(&Chain);
        }
    }
    else
    {
        printf(" tiling daemon: no match for '%.*s'\n", Type.Length, Type.Text);
    }
}
