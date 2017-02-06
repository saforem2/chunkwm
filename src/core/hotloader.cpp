#include "hotloader.h"

#include <string.h>
#include <vector>

#define internal static

#define HOTLOADER_CALLBACK(name) void name(ConstFSEventStreamRef Stream,\
                                           void *Context,\
                                           size_t Count,\
                                           void *Paths,\
                                           const FSEventStreamEventFlags *Flags,\
                                           const FSEventStreamEventId *Ids)

internal bool
FileHasExtensionSO(char *File)
{
    bool Result;

    char *Extension = strrchr(File, '.');
    if(Extension)
    {
        Result = (strcmp(Extension, ".so") == 0);
    }
    else
    {
        Result = false;
    }

    return Result;
}

internal
HOTLOADER_CALLBACK(HotloadPluginCallback)
{
    char **Files = (char **) Paths;

    for(size_t Index = 0;
        Index < Count;
        ++Index)
    {
        // NOTE(koekeishiya): We also receive notifications for subdirectories.
        char *File = Files[Index];
        if(FileHasExtensionSO(File))
        {
            // TODO(koekeishiya): reload plugin..
            printf("hotloader: '%s' changed!\n", File);
        }
    }
}

internal hotloader Hotloader;
internal std::vector<const char *> Directories;

void HotloaderAddPath(const char *Path)
{
    if(!Hotloader.Enabled)
    {
        Directories.push_back(Path);
    }
}

void HotloaderInit()
{
    if(!Hotloader.Enabled)
    {
        int Count = Directories.size();
        CFStringRef StringRefs[Count];

        for(size_t Index = 0;
            Index < Count;
            ++Index)
        {
            StringRefs[Index] = CFStringCreateWithCString(kCFAllocatorDefault,
                                                          Directories[Index],
                                                          kCFStringEncodingUTF8);
        }

        Hotloader.Enabled = true;
        Hotloader.Path = (CFArrayRef) CFArrayCreate(NULL, (const void **) StringRefs, Count, &kCFTypeArrayCallBacks);

        Hotloader.Flags = kFSEventStreamCreateFlagNoDefer |
                          kFSEventStreamCreateFlagFileEvents;

        Hotloader.Stream = FSEventStreamCreate(NULL,
                                               HotloadPluginCallback,
                                               NULL,
                                               Hotloader.Path,
                                               kFSEventStreamEventIdSinceNow,
                                               0.5,
                                               Hotloader.Flags);

        FSEventStreamScheduleWithRunLoop(Hotloader.Stream, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        FSEventStreamStart(Hotloader.Stream);
    }
}

void HotloaderTerminate()
{
    if(Hotloader.Enabled)
    {
        FSEventStreamStop(Hotloader.Stream);
        FSEventStreamInvalidate(Hotloader.Stream);
        FSEventStreamRelease(Hotloader.Stream);

        CFIndex Count = CFArrayGetCount(Hotloader.Path);
        for(size_t Index = 0;
            Index < Count;
            ++Index)
        {
            CFStringRef StringRef = (CFStringRef) CFArrayGetValueAtIndex(Hotloader.Path, Index);
            CFRelease(StringRef);
        }

        CFRelease(Hotloader.Path);
        Hotloader.Enabled = false;
        Directories.clear();
    }
}
