#include "vspace.h"
#include "node.h"
#include "constants.h"
#include "misc.h"

#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/misc/assert.h"
#include "../../common/config/cvar.h"

#include <stdlib.h>
#include <pthread.h>

#define internal static
#define local_persist static

internal virtual_space_map VirtualSpaces;
internal pthread_mutex_t VirtualSpacesLock;

internal virtual_space_mode
VirtualSpaceModeFromString(char *Value)
{
    for (int Index = Virtual_Space_Bsp; Index <= Virtual_Space_Float; ++Index) {
        if (strcmp(Value, virtual_space_mode_str[Index]) == 0) {
            return (virtual_space_mode) Index;
        }
    }

    return Virtual_Space_Bsp;
}

internal virtual_space_config
GetVirtualSpaceConfig(unsigned SpaceIndex)
{
    virtual_space_config Config;

    char KeyMode[BUFFER_SIZE];
    snprintf(KeyMode, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_MODE);
    Config.Mode = CVarExists(KeyMode) ? VirtualSpaceModeFromString(CVarStringValue(KeyMode))
                                      : VirtualSpaceModeFromString(CVarStringValue(CVAR_SPACE_MODE));
    char KeyTop[BUFFER_SIZE];
    snprintf(KeyTop, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_OFFSET_TOP);
    Config.Offset.Top = CVarExists(KeyTop) ? CVarFloatingPointValue(KeyTop)
                                           : CVarFloatingPointValue(CVAR_SPACE_OFFSET_TOP);
    char KeyBottom[BUFFER_SIZE];
    snprintf(KeyBottom, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_OFFSET_BOTTOM);
    Config.Offset.Bottom = CVarExists(KeyBottom) ? CVarFloatingPointValue(KeyBottom)
                                                 : CVarFloatingPointValue(CVAR_SPACE_OFFSET_BOTTOM);
    char KeyLeft[BUFFER_SIZE];
    snprintf(KeyLeft, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_OFFSET_LEFT);
    Config.Offset.Left = CVarExists(KeyLeft) ? CVarFloatingPointValue(KeyLeft)
                                             : CVarFloatingPointValue(CVAR_SPACE_OFFSET_LEFT);
    char KeyRight[BUFFER_SIZE];
    snprintf(KeyRight, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_OFFSET_RIGHT);
    Config.Offset.Right = CVarExists(KeyRight) ? CVarFloatingPointValue(KeyRight)
                                               : CVarFloatingPointValue(CVAR_SPACE_OFFSET_RIGHT);
    char KeyGap[BUFFER_SIZE];
    snprintf(KeyGap, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_OFFSET_GAP);
    Config.Offset.Gap = CVarExists(KeyGap) ? CVarFloatingPointValue(KeyGap)
                                           : CVarFloatingPointValue(CVAR_SPACE_OFFSET_GAP);
    char KeyTree[BUFFER_SIZE];
    snprintf(KeyTree, BUFFER_SIZE, "%d_%s", SpaceIndex, _CVAR_SPACE_TREE);
    Config.TreeLayout = CVarExists(KeyTree) ? CVarStringValue(KeyTree)
                                            : NULL;
    return Config;
}

internal virtual_space *
CreateAndInitVirtualSpace(macos_space *Space)
{
    virtual_space *VirtualSpace = (virtual_space *) malloc(sizeof(virtual_space));
    VirtualSpace->Tree = NULL;
    VirtualSpace->Preselect = NULL;

    // TODO(koekeishiya): How do we react if this call fails ??
    bool Mutex = pthread_mutex_init(&VirtualSpace->Lock, NULL) == 0;
    ASSERT(Mutex);

    // NOTE(koekeishiya): The monitor arrangement is not necessary here.
    // We are able to address spaces using mission-control indexing.
    unsigned DesktopId = 1;
    bool Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);

    virtual_space_config Config = GetVirtualSpaceConfig(DesktopId);
    VirtualSpace->Mode = Config.Mode;
    VirtualSpace->TreeLayout = Config.TreeLayout;
    VirtualSpace->_Offset = Config.Offset;
    VirtualSpace->Offset = &VirtualSpace->_Offset;

    return VirtualSpace;
}

bool ShouldDeserializeVirtualSpace(virtual_space *VirtualSpace)
{
    bool Result = ((VirtualSpace->Mode == Virtual_Space_Bsp) &&
                   (VirtualSpace->TreeLayout) &&
                   (FileExists(VirtualSpace->TreeLayout)));
    return Result;
}

bool VirtualSpaceHasFlags(virtual_space *VirtualSpace, uint32_t Flag)
{
    bool Result = ((VirtualSpace->Flags & Flag) != 0);
    return Result;
}

void VirtualSpaceAddFlags(virtual_space *VirtualSpace, uint32_t Flag)
{
    VirtualSpace->Flags |= Flag;
}

void VirtualSpaceClearFlags(virtual_space *VirtualSpace, uint32_t Flag)
{
    VirtualSpace->Flags &= ~Flag;
}

// NOTE(koekeishiya): If the requested space does not exist, we create it.
virtual_space *AcquireVirtualSpace(macos_space *Space)
{
    virtual_space *VirtualSpace;

    char *SpaceCRef = CopyCFStringToC(Space->Ref);
    ASSERT(SpaceCRef);

    pthread_mutex_lock(&VirtualSpacesLock);
    virtual_space_map_it It = VirtualSpaces.find(SpaceCRef);
    if (It != VirtualSpaces.end()) {
        VirtualSpace = It->second;
        free(SpaceCRef);
    } else {
        VirtualSpace = CreateAndInitVirtualSpace(Space);
        VirtualSpaces[SpaceCRef] = VirtualSpace;
    }
    pthread_mutex_unlock(&VirtualSpacesLock);

    pthread_mutex_lock(&VirtualSpace->Lock);
    return VirtualSpace;
}

void ReleaseVirtualSpace(virtual_space *VirtualSpace)
{
    pthread_mutex_unlock(&VirtualSpace->Lock);
}

bool BeginVirtualSpaces()
{
    return pthread_mutex_init(&VirtualSpacesLock, NULL) == 0;
}

void EndVirtualSpaces()
{
    for (virtual_space_map_it It = VirtualSpaces.begin(); It != VirtualSpaces.end(); ++It) {
        virtual_space *VirtualSpace = It->second;

        if (VirtualSpace->Tree) {
            FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        }

        pthread_mutex_destroy(&VirtualSpace->Lock);
        free(VirtualSpace);
        free((char *) It->first);
    }

    VirtualSpaces.clear();
    pthread_mutex_destroy(&VirtualSpacesLock);
}

void VirtualSpaceRecreateRegions(macos_space *Space, virtual_space *VirtualSpace)
{
    CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
    CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
    VirtualSpaceClearFlags(VirtualSpace, Virtual_Space_Require_Resize);
}

void VirtualSpaceUpdateRegions(virtual_space *VirtualSpace)
{
    ApplyNodeRegionWithPotentialZoom(VirtualSpace->Tree, VirtualSpace);
    VirtualSpaceClearFlags(VirtualSpace, Virtual_Space_Require_Region_Update);
}
