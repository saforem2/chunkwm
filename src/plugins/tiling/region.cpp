#include "region.h"
#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"

#define internal static

#define OSX_MENU_BAR_HEIGHT 20.0f
internal region
FullscreenRegion()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region_offset *Offset = VirtualSpace->Offset;

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    CGRect Display = AXLibGetDisplayBounds(DisplayRef);
    CFRelease(DisplayRef);

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

    if(!AXLibIsMenuBarAutoHideEnabled())
    {
        Result.Y += OSX_MENU_BAR_HEIGHT;
        Result.Height -= OSX_MENU_BAR_HEIGHT;
    }

    AXLibDestroySpace(Space);
    return Result;
}

internal region
LeftVerticalRegion(node *Node)
{
    ASSERT(Node);

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

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
RightVerticalRegion(node *Node)
{
    ASSERT(Node);

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

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
UpperHorizontalRegion(node *Node)
{
    ASSERT(Node);

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

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
LowerHorizontalRegion(node *Node)
{
    ASSERT(Node);

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

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
    switch(Type)
    {
        case Region_Full:
        {
            Node->Region = FullscreenRegion();
        } break;
        case Region_Left:
        {
            Node->Region = LeftVerticalRegion(Node->Parent);
        } break;
        case Region_Right:
        {
            Node->Region = RightVerticalRegion(Node->Parent);
        } break;
        case Region_Upper:
        {
            Node->Region = UpperHorizontalRegion(Node->Parent);
        } break;
        case Region_Lower:
        {
            Node->Region = LowerHorizontalRegion(Node->Parent);
        } break;
        default: { /* NOTE(koekeishiya): Invalid region specified. */} break;
    }

    Node->Region.Type = Type;
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
