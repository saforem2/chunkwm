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
    else if(Flags & kCGDisplayRemoveFlag)
    {
        printf("%d: Display removed\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayRemoved, Context);
    }
    else if(Flags & kCGDisplayMovedFlag)
    {
        printf("%d: Display moved\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayMoved, Context);
    }
    else if(Flags & kCGDisplayDesktopShapeChangedFlag)
    {
        printf("%d: Display resolution changed\n", DisplayId);
        ConstructEvent(ChunkWM_DisplayResized, Context);
    }
    else
    {
        free(Context);
    }
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
