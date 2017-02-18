#include "vspace.h"
#include "node.h"

#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>

#define internal static

internal virtual_space_map VirtualSpaces;
internal virtual_space_config_map VirtualSpacesConfig;

internal virtual_space_config *
GetVirtualSpaceConfig(int DisplayIndex, int SpaceIndex)
{
    virtual_space_identifier Key = { DisplayIndex, SpaceIndex };
    virtual_space_config_map_it It = VirtualSpacesConfig.find(Key);

    virtual_space_config Config = {};

    // NOTE(koekeishiya): This virtual_space has space specific config.
    if(It != VirtualSpacesConfig.end())
    {
        return &It->second;
    }
    else
    {
        // NOTE(koekeishiya): This virtual_space does not have space specific config.
        // TODO(koekeishiya): Check for display specific config.
    }

    return NULL;
}

internal virtual_space *
CreateAndInitVirtualSpace(macos_display *Display, macos_space *Space)
{
    virtual_space *VirtualSpace = (virtual_space *) malloc(sizeof(virtual_space));

    VirtualSpace->Tree = NULL;

    unsigned DesktopId = AXLibCGSSpaceIDToDesktopID(Display->Ref, Space->Id);
    virtual_space_config *Config = GetVirtualSpaceConfig(Display->Arrangement, DesktopId);

    // TODO(koekeishiya): GetVirtualSpaceConfig(..) should not be able to return null!
    if(Config)
    {
        // TODO(koekeishiya): Do we want the virtual_space members to be pointers ?
        VirtualSpace->Mode = Config->Mode;
        VirtualSpace->Offset = Config->Offset;
    }
    else
    {
        internal region_offset Offset = { 60, 50, 50, 50, 20 };
        VirtualSpace->Mode = Virtual_Space_Bsp;
        VirtualSpace->Offset = Offset;
    }

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
