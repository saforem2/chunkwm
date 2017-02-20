#include "vspace.h"
#include "node.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>

#define internal static
#define local_persist static

internal virtual_space_map VirtualSpaces;

internal virtual_space_config
GetVirtualSpaceConfig(int DisplayIndex, int SpaceIndex)
{
    virtual_space_config Config;

    char KeyMode[BUFFER_SIZE];
    snprintf(KeyMode, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_MODE);
    Config.Mode = CVarExists(KeyMode) ? (virtual_space_mode) CVarIntegerValue(KeyMode)
                                      : (virtual_space_mode) CVarIntegerValue(CVAR_SPACE_MODE);
    char KeyTop[BUFFER_SIZE];
    snprintf(KeyTop, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_OFFSET_TOP);
    Config.Offset.Top = CVarExists(KeyTop) ? CVarFloatingPointValue(KeyTop)
                                           : CVarFloatingPointValue(CVAR_SPACE_OFFSET_TOP);
    char KeyBottom[BUFFER_SIZE];
    snprintf(KeyBottom, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_OFFSET_BOTTOM);
    Config.Offset.Bottom = CVarExists(KeyBottom) ? CVarFloatingPointValue(KeyBottom)
                                                 : CVarFloatingPointValue(CVAR_SPACE_OFFSET_BOTTOM);
    char KeyLeft[BUFFER_SIZE];
    snprintf(KeyLeft, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_OFFSET_LEFT);
    Config.Offset.Left = CVarExists(KeyLeft) ? CVarFloatingPointValue(KeyLeft)
                                             : CVarFloatingPointValue(CVAR_SPACE_OFFSET_LEFT);
    char KeyRight[BUFFER_SIZE];
    snprintf(KeyRight, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_OFFSET_RIGHT);
    Config.Offset.Right = CVarExists(KeyRight) ? CVarFloatingPointValue(KeyRight)
                                               : CVarFloatingPointValue(CVAR_SPACE_OFFSET_RIGHT);
    char KeyGap[BUFFER_SIZE];
    snprintf(KeyGap, BUFFER_SIZE, "%d_%d_%s", DisplayIndex, SpaceIndex, _CVAR_SPACE_OFFSET_GAP);
    Config.Offset.Gap = CVarExists(KeyGap) ? CVarFloatingPointValue(KeyGap)
                                           : CVarFloatingPointValue(CVAR_SPACE_OFFSET_GAP);
    return Config;
}

internal virtual_space *
CreateAndInitVirtualSpace(macos_display *Display, macos_space *Space)
{
    virtual_space *VirtualSpace = (virtual_space *) malloc(sizeof(virtual_space));
    VirtualSpace->Tree = NULL;

    unsigned DesktopId = AXLibCGSSpaceIDToDesktopID(Display->Ref, Space->Id);
    virtual_space_config Config = GetVirtualSpaceConfig(Display->Arrangement, DesktopId);

    VirtualSpace->Mode = Config.Mode;
    VirtualSpace->Offset = Config.Offset;

    return VirtualSpace;
}

// NOTE(koekeishiya): If the requested space does not exist, we create it.
virtual_space *AcquireVirtualSpace(macos_display *Display, macos_space *Space)
{
    virtual_space *Result;

    char *SpaceCRef = CopyCFStringToC(Space->Ref);
    ASSERT(SpaceCRef);

    virtual_space_map_it It = VirtualSpaces.find(SpaceCRef);
    if(It != VirtualSpaces.end())
    {
        Result = It->second;
        free(SpaceCRef);
    }
    else
    {
        Result = CreateAndInitVirtualSpace(Display, Space);
        VirtualSpaces[SpaceCRef] = Result;
    }

    return Result;
}

void ReleaseVirtualSpaces()
{
    for(virtual_space_map_it It = VirtualSpaces.begin();
        It != VirtualSpaces.end();
        ++It)
    {
        virtual_space *VirtualSpace = It->second;

        if(VirtualSpace->Tree)
        {
            FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        }

        free(VirtualSpace);
        free((char *) It->first);
    }

    VirtualSpaces.clear();
}
