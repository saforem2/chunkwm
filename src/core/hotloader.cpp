#include "hotloader.h"

#include "plugin.h"
#include "constants.h"

#include <sys/stat.h>
#include <string.h>
#include <vector>

#include "../common/ipc/daemon.h"
#include "../common/misc/debug.h"

#define internal static

#define HOTLOADER_CALLBACK(name) void name(ConstFSEventStreamRef Stream,\
                                           void *Context,\
                                           size_t Count,\
                                           void *Paths,\
                                           const FSEventStreamEventFlags *Flags,\
                                           const FSEventStreamEventId *Ids)

internal hotloader Hotloader;
internal std::vector<const char *> Directories;

internal void
PerformIOOperation(const char *Op, char *Filename)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, CHUNKWM_PORT))
    {
        char Message[256];
        Message[0] = '\0';
        snprintf(Message, sizeof(Message), "%s %s", Op, Filename);
        WriteToSocket(Message, SockFD);
        CloseSocket(SockFD);
    }
}

internal bool
WatchedIODirectory(char *Absolutepath, char **Filename)
{
    bool Success = false;
    char *LastSlash = strrchr(Absolutepath, '/');
    if(LastSlash)
    {
        // NOTE(koekeisihya): Null terminate '/' to cut off filename
        *LastSlash = '\0';

        // NOTE(koekeishiya): We receive notifications for subdirectories, skip these.
        for(size_t Index = 0; Index < Directories.size(); ++Index)
        {
            if(strcmp(Absolutepath, Directories[Index]) == 0)
            {
                *Filename = LastSlash + 1;
                Success = true;
                break;
            }
        }

        // NOTE(koekeisihya): Revert '/' to restore filename
        *LastSlash = '/';
    }

    return Success;
}

internal char *
WatchedIOFileChange(char *Absolutepath)
{
    char *Filename;
    if(WatchedIODirectory(Absolutepath, &Filename))
    {
        char *Extension = strrchr(Filename, '.');
        if((Extension) && (strcmp(Extension, ".so") == 0))
        {
            return Filename;
        }
    }

    return NULL;
}

internal
HOTLOADER_CALLBACK(HotloadPluginCallback)
{
    char **Files = (char **) Paths;

    for(size_t Index = 0; Index < Count; ++Index)
    {
        char *Absolutepath = Files[Index];
        char *Filename;

        if((Filename = WatchedIOFileChange(Absolutepath)))
        {
            printf("hotloader: plugin '%s' changed!\n", Filename);

            DEBUG_PRINT("hotloader: unloading plugin '%s'\n", Filename);
            PerformIOOperation("core::unload", Filename);

            struct stat Buffer;
            if(stat(Absolutepath, &Buffer) == 0)
            {
                DEBUG_PRINT("hotloader: loading plugin '%s'\n", Filename);
                PerformIOOperation("core::load", Filename);
            }
        }
    }
}

void HotloaderAddPath(const char *Path)
{
    if(!Hotloader.Enabled)
    {
        struct stat Buffer;
        if(lstat(Path, &Buffer) == 0)
        {
            if(S_ISDIR(Buffer.st_mode))
            {
                // NOTE(koekeishiya): not a symlink.
                Directories.push_back(Path);
            }
            else if(S_ISLNK(Buffer.st_mode))
            {
                ssize_t Size = Buffer.st_size + 1;
                char Directory[Size];
                ssize_t Result = readlink(Path, Directory, Size);

                if(Result != -1)
                {
                    Directory[Result] = '\0';
                    Directories.push_back(strdup(Directory));
                    printf("hotloader: symlink '%s' -> '%s'\n", Path, Directory);
                }
            }
            else
            {
                fprintf(stderr, "hotloader: '%s' is not a directory!\n", Path);
            }
        }
        else
        {
            fprintf(stderr, "hotloader: '%s' is not a valid path!\n", Path);
        }
    }
}

void HotloaderInit()
{
    if(!Hotloader.Enabled)
    {
        int Count = Directories.size();
        if(Count)
        {
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
        else
        {
            fprintf(stderr, "hotloader: no directories specified!\n");
        }
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
