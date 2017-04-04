#include "region.h"
#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

#define OSX_MENU_BAR_HEIGHT 20.0f
internal region
FullscreenRegion(CFStringRef DisplayRef)
{
    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;

    CGRect Display = AXLibGetDisplayBounds(DisplayRef);

    region Result;
    Result.X = Display.origin.x;
    Result.Y = Display.origin.y;
    Result.Width = Display.size.width;
    Result.Height = Display.size.height;

    if(Offset)
    {
        Result.X += Offset->Left;
        Result.Y += Offset->Top;
        Result.Width -= (Offset->Left + Offset->Right);
        Result.Height -= (Offset->Top + Offset->Bottom);
    }

    // NOTE(koekeishiya): Automatically adjust padding to account for osx menubar status.
    if(!AXLibIsMenuBarAutoHideEnabled())
    {
        Result.Y += OSX_MENU_BAR_HEIGHT;
        Result.Height -= OSX_MENU_BAR_HEIGHT;
    }

    /* TODO(koekeishiya): is the Dock only ever visible on the primary display when autohide is disabled ?
     * We need to figure out if the display that we are creating our region for should apply
     * padding for the Dock or not. Right now it is applied to all displays !
     *
     * Revisit when doing multi-monitor support !!! */
    if(!AXLibIsDockAutoHideEnabled())
    {
        macos_dock_orientation Orientation = AXLibGetDockOrientation();
        size_t TileSize = AXLibGetDockTileSize() + 16;

        switch(Orientation)
        {
            case Dock_Orientation_Left:
            {
                Result.X += TileSize;
                Result.Width -= TileSize;
            } break;
            case Dock_Orientation_Right:
            {
                Result.Width -= TileSize;
            } break;
            case Dock_Orientation_Bottom:
            {
                Result.Height -= TileSize;
            } break;
        }
    }

    AXLibDestroySpace(Space);
    return Result;
}

internal region
LeftVerticalRegion(CFStringRef DisplayRef, node *Node)
{
    ASSERT(Node);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;
    region *Region = &Node->Region;

    region Result;
    Result.X = Region->X;
    Result.Y = Region->Y;
    Result.Width = Region->Width * Node->Ratio;
    Result.Height = Region->Height;

    if(Offset)
    {
        Result.Width -= (Offset->Gap / 2);
    }

    AXLibDestroySpace(Space);
    return Result;
}

internal region
RightVerticalRegion(CFStringRef DisplayRef, node *Node)
{
    ASSERT(Node);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;
    region *Region = &Node->Region;

    region Result;
    Result.X = Region->X + (Region->Width * Node->Ratio);
    Result.Y = Region->Y;
    Result.Width = Region->Width * (1 - Node->Ratio);
    Result.Height = Region->Height;

    if(Offset)
    {
        Result.X += (Offset->Gap / 2);
        Result.Width -= (Offset->Gap / 2);
    }

    AXLibDestroySpace(Space);
    return Result;
}

internal region
UpperHorizontalRegion(CFStringRef DisplayRef, node *Node)
{
    ASSERT(Node);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;
    region *Region = &Node->Region;

    region Result;
    Result.X = Region->X;
    Result.Y = Region->Y;
    Result.Width = Region->Width;
    Result.Height = Region->Height * Node->Ratio;

    if(Offset)
    {
        Result.Height -= (Offset->Gap / 2);
    }

    AXLibDestroySpace(Space);
    return Result;
}

internal region
LowerHorizontalRegion(CFStringRef DisplayRef, node *Node)
{
    ASSERT(Node);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;
    region *Region = &Node->Region;

    region Result;
    Result.X = Region->X;
    Result.Y = Region->Y + (Region->Height * Node->Ratio);
    Result.Width = Region->Width;
    Result.Height = Region->Height * (1 - Node->Ratio);

    if(Offset)
    {
        Result.Y += (Offset->Gap / 2);
        Result.Height -= (Offset->Gap / 2);
    }

    AXLibDestroySpace(Space);
    return Result;
}

void CreateNodeRegion(node *Node, region_type Type)
{
    ASSERT(Type >= Region_Full && Type <= Region_Lower);

    // NOTE(koekeishiya): This private API appears to be inconsistent
    // as it does not return a value for all windows. Fails for 'mpv.app'
    // and probably others.
    // CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(Node->WindowId);

    macos_window *Window = GetWindowByID(Node->WindowId);
    ASSERT(Window);

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    switch(Type)
    {
        case Region_Full:
        {
            Node->Region = FullscreenRegion(DisplayRef);
        } break;
        case Region_Left:
        {
            Node->Region = LeftVerticalRegion(DisplayRef, Node->Parent);
        } break;
        case Region_Right:
        {
            Node->Region = RightVerticalRegion(DisplayRef, Node->Parent);
        } break;
        case Region_Upper:
        {
            Node->Region = UpperHorizontalRegion(DisplayRef, Node->Parent);
        } break;
        case Region_Lower:
        {
            Node->Region = LowerHorizontalRegion(DisplayRef, Node->Parent);
        } break;
        default: { /* NOTE(koekeishiya): Invalid region specified. */} break;
    }

    Node->Region.Type = Type;
    CFRelease(DisplayRef);
}

internal void
CreateNodeRegionPair(node *Left, node *Right, node_split Split)
{
    ASSERT(Split == Split_Vertical || Split == Split_Horizontal);
    if(Split == Split_Vertical)
    {
        CreateNodeRegion(Left, Region_Left);
        CreateNodeRegion(Right, Region_Right);
    }
    else if (Split == Split_Horizontal)
    {
        CreateNodeRegion(Left, Region_Upper);
        CreateNodeRegion(Right, Region_Lower);
    }
}

void ResizeNodeRegion(node *Node)
{
    if(Node)
    {
        if(Node->Left)
        {
            CreateNodeRegion(Node->Left, Node->Left->Region.Type);
            ResizeNodeRegion(Node->Left);
        }

        if(Node->Right)
        {
            CreateNodeRegion(Node->Right, Node->Right->Region.Type);
            ResizeNodeRegion(Node->Right);
        }
    }
}

void CreateNodeRegionRecursive(node *Node, bool Optimal)
{
    if(Node && Node->Left && Node->Right)
    {
        Node->Split = Optimal ? OptimalSplitMode(Node) : Node->Split;
        CreateNodeRegionPair(Node->Left, Node->Right, Node->Split);

        CreateNodeRegionRecursive(Node->Left, Optimal);
        CreateNodeRegionRecursive(Node->Right, Optimal);
    }
}
