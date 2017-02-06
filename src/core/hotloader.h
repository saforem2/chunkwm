#ifndef CHUNKWM_CORE_HOTLOADER_H
#define CHUNKWM_CORE_HOTLOADER_H

#include <Carbon/Carbon.h>

struct hotloader
{
    FSEventStreamRef Stream;
    FSEventStreamEventFlags Flags;
    CFArrayRef Path;
    bool Enabled;
};

void HotloaderInit();
void HotloaderTerminate();

void HotloaderRegisterDirectory(const char *Path);

#endif
