#ifndef CHUNKWM_CORE_CONFIG_H
#define CHUNKWM_CORE_CONFIG_H

struct chunkwm_delegate
{
    int SockFD;
    char *Target;
    char *Command;
    const char *Message;
};

#endif
