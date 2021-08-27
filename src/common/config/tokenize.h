#ifndef CHUNKWM_COMMON_TOKENIZE_H
#define CHUNKWM_COMMON_TOKENIZE_H

struct token
{
    const char *Text;
    unsigned Length;
};

bool TokenEquals(token Token, const char *Match);
char *TokenToString(token Token);
float TokenToFloat(token Token);
int TokenToInt(token Token);
unsigned TokenToUnsigned(token Token);
bool TokenIsDigit(token Token);

// NOTE(koekeishiya): simple 'whitespace' tokenizer
token GetToken(const char **Data);
#endif
