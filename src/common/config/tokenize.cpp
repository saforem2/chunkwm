#include "tokenize.h"

#include "../misc/assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool TokenEquals(token Token, const char *Match)
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

char *TokenToString(token Token)
{
    char *Result = (char *) malloc(Token.Length + 1);
    Result[Token.Length] = '\0';
    memcpy(Result, Token.Text, Token.Length);
    return Result;
}

float TokenToFloat(token Token)
{
    float Result = 0.0f;
    char *String = TokenToString(Token);
    sscanf(String, "%f", &Result);
    free(String);
    return Result;
}

int TokenToInt(token Token)
{
    int Result = 0;
    char *String = TokenToString(Token);
    sscanf(String, "%d", &Result);
    free(String);
    return Result;
}

// NOTE(koekeishiya): simple 'whitespace' tokenizer
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
