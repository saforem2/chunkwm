#include "config.h"
#include "vspace.h"
#include "node.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    ++(*Data);

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

/* NOTE(koekeishiya): Parameters
 *
 * const char *Message
 * int SockFD
 *
 * */
DAEMON_CALLBACK(DaemonCallback)
{
    printf(" tiling daemon: '%s'\n", Message);
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
        printf(" tiling daemon: no match: '%.*s'\n", Command.Length, Command.Text);
    }
}
