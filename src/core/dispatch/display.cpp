#include "display.h"
#include "event.h"

#include <Carbon/Carbon.h>

#define internal static

/* NOTE(koekeishiya): Get notified about display changes. */
internal void
DisplayCallback(CGDirectDisplayID DisplayId, CGDisplayChangeSummaryFlags Flags, void *Reference)
{
    CGDirectDisplayID *Context = (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID));
    *Context = DisplayId;

    if(Flags & kCGDisplayAddFlag)
    {
        printf("%d: Display added\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayAdded, Context);
    }

    if(Flags & kCGDisplayRemoveFlag)
    {
        printf("%d: Display removed\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayRemoved, Context);
    }

    if(Flags & kCGDisplayMovedFlag)
    {
        printf("%d: Display moved\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayMoved, Context);
    }

    if(Flags & kCGDisplayDesktopShapeChangedFlag)
    {
        printf("%d: Display resolution changed\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayResized, Context);
    }

#if 0
    if(Flags & kCGDisplaySetMainFlag)
    {
        printf("%d: Display became main\n", DisplayID);
    }

    if(Flags & kCGDisplaySetModeFlag)
    {
        printf("%d: Display changed mode\n", DisplayID);
    }

    if(Flags & kCGDisplayEnabledFlag)
    {
        printf("%d: Display enabled\n", DisplayID);
    }

    if(Flags & kCGDisplayDisabledFlag)
    {
        printf("%d: Display disabled\n", DisplayID);
    }
#endif
}

bool BeginDisplayHandler()
{
    CGError Error = CGDisplayRegisterReconfigurationCallback(DisplayCallback, NULL);
    return (Error == kCGErrorSuccess);
}

bool EndDisplayHandler()
{
    CGError Error = CGDisplayRemoveReconfigurationCallback(DisplayCallback, NULL);
    return (Error == kCGErrorSuccess);
}
