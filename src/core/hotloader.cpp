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

internal hotloader Hotloader;
internal std::vector<const char *> Directories;

internal
HOTLOADER_CALLBACK(HotloadPluginCallback)
{
    char **Files = (char **) Paths;

    for(size_t Index = 0;
        Index < Count;
        ++Index)
    {
        char *File = Files[Index];
        bool ValidDirectory = false;

        char *LastSlash = strrchr(File, '/');
        if(LastSlash)
        {
            *LastSlash = '\0';

            // NOTE(koekeishiya): We receive notifications for subdirectories, skip these.
            for(size_t Index = 0;
                Index < Directories.size();
                ++Index)
            {
                if(strcmp(File, Directories[Index]) == 0)
                {
                    ValidDirectory = true;
                    break;
                }
            }

            *LastSlash = '/';
        }

        if(ValidDirectory)
        {
            char *Extension = strrchr(File, '.');
            if((Extension) && (strcmp(Extension, ".so") == 0))
            {
                // TODO(koekeishiya): reload plugin..
                printf("hotloader: '%s' changed!\n", File);
            }
        }
    }
}

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
