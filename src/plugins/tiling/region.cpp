#include "region.h"
#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

region CGRectToRegion(CGRect Rect)
{
    region Result = { (float) Rect.origin.x,   (float) Rect.origin.y,
                      (float) Rect.size.width, (float) Rect.size.height,
                      Region_Full };
    return Result;
}

region RoundPreselRegion(region Region, CGPoint Position, CGSize Size)
{
    region Result = {
        roundf(Region.X),
        roundf(Region.Y),
        roundf(Region.Width),
        roundf(Region.Height)
    };

    float DiffX = abs(Region.X - Position.x);
    float DiffY = abs(Region.Y - Position.y);
    float DiffW = abs(Region.Width - Size.width);
    float DiffH = abs(Region.Height - Size.height);

    if ((DiffX >= 0.5) && (DiffX <= 2.5)) {
        Result.X = Position.x;
    }

    if ((DiffY >= 0.5) && (DiffY <= 2.5)) {
        Result.Y = Position.y;
    }

    if ((DiffW >= 0.5) && (DiffW <= 2.5)) {
        Result.Width = Size.width;
    }

    if ((DiffH >= 0.5) && (DiffH <= 2.5)) {
        Result.Height = Size.height;
    }

    return Result;
}

#define OSX_MENU_BAR_HEIGHT 22.0f
void ConstrainRegion(CFStringRef DisplayRef, region *Region)
{
    // NOTE(koekeishiya): Automatically adjust padding to account for osx menubar status.
    if (!AXLibIsMenuBarAutoHideEnabled()) {
        Region->Y += OSX_MENU_BAR_HEIGHT;
        Region->Height -= OSX_MENU_BAR_HEIGHT;
    }

    if (CVarIntegerValue(CVAR_BAR_ENABLED)) {
        bool ShouldApplyOffset = true;
        if (!CVarIntegerValue(CVAR_BAR_ALL_MONITORS)) {
            CFStringRef MainDisplayRef = AXLibGetDisplayIdentifierForMainDisplay();
            ASSERT(MainDisplayRef);

            if (CFStringCompare(DisplayRef, MainDisplayRef, 0) != kCFCompareEqualTo) {
                ShouldApplyOffset = false;
            }

            CFRelease(MainDisplayRef);
        }

        if (ShouldApplyOffset) {
            Region->X += CVarFloatingPointValue(CVAR_BAR_OFFSET_LEFT);
            Region->Width -= CVarFloatingPointValue(CVAR_BAR_OFFSET_LEFT);

            Region->Y += CVarFloatingPointValue(CVAR_BAR_OFFSET_TOP);
            Region->Height -= CVarFloatingPointValue(CVAR_BAR_OFFSET_TOP);

            Region->Width -= CVarFloatingPointValue(CVAR_BAR_OFFSET_RIGHT);
            Region->Height -= CVarFloatingPointValue(CVAR_BAR_OFFSET_BOTTOM);
        }
    }

    if (!AXLibIsDockAutoHideEnabled()) {
        macos_dock_orientation Orientation = AXLibGetDockOrientation();
        CGRect DockRect = AXLibGetDockRect();

        switch (Orientation) {
        case Dock_Orientation_Left: {
            CFStringRef LeftMostDisplayRef = AXLibGetDisplayIdentifierForLeftMostDisplay();
            ASSERT(LeftMostDisplayRef);

            if (CFStringCompare(DisplayRef, LeftMostDisplayRef, 0) == kCFCompareEqualTo) {
                Region->X += DockRect.size.width;
                Region->Width -= DockRect.size.width;
            }

            CFRelease(LeftMostDisplayRef);
        } break;
        case Dock_Orientation_Right: {
            CFStringRef RightMostDisplayRef = AXLibGetDisplayIdentifierForRightMostDisplay();
            ASSERT(RightMostDisplayRef);

            if (CFStringCompare(DisplayRef, RightMostDisplayRef, 0) == kCFCompareEqualTo) {
                Region->Width -= DockRect.size.width;
            }

            CFRelease(RightMostDisplayRef);
        } break;
        case Dock_Orientation_Bottom: {
            CFStringRef BottomMostDisplayRef = AXLibGetDisplayIdentifierForBottomMostDisplay();
            ASSERT(BottomMostDisplayRef);

            if (CFStringCompare(DisplayRef, BottomMostDisplayRef, 0) == kCFCompareEqualTo) {
                Region->Height -= DockRect.size.height;
            }

            CFRelease(BottomMostDisplayRef);
        } break;
        case Dock_Orientation_Top: { /* NOTE(koekeishiya) compiler warning.. */ } break;
        }
    }
}

region FullscreenRegion(CFStringRef DisplayRef, virtual_space *VirtualSpace)
{
    region Result = CGRectToRegion(AXLibGetDisplayBounds(DisplayRef));
    ConstrainRegion(DisplayRef, &Result);

    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.X += Offset->Left;
        Result.Y += Offset->Top;
        Result.Width -= (Offset->Left + Offset->Right);
        Result.Height -= (Offset->Top + Offset->Bottom);
    }

    return Result;
}

internal region
LeftVerticalRegion(node *Node, virtual_space *VirtualSpace)
{
    ASSERT(Node);

    region *Region = &Node->Region;
    region Result;

    Result.X = Region->X;
    Result.Y = Region->Y;
    Result.Width = Region->Width * Node->Ratio;
    Result.Height = Region->Height;

    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.Width -= (Offset->Gap / 2);
    }

    return Result;
}

internal region
RightVerticalRegion(node *Node, virtual_space *VirtualSpace)
{
    ASSERT(Node);

    region *Region = &Node->Region;
    region Result;

    Result.X = Region->X + (Region->Width * Node->Ratio);
    Result.Y = Region->Y;
    Result.Width = Region->Width * (1 - Node->Ratio);
    Result.Height = Region->Height;

    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.X += (Offset->Gap / 2);
        Result.Width -= (Offset->Gap / 2);
    }

    return Result;
}

internal region
UpperHorizontalRegion(node *Node, virtual_space *VirtualSpace)
{
    ASSERT(Node);

    region *Region = &Node->Region;
    region Result;

    Result.X = Region->X;
    Result.Y = Region->Y;
    Result.Width = Region->Width;
    Result.Height = Region->Height * Node->Ratio;

    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.Height -= (Offset->Gap / 2);
    }

    return Result;
}

internal region
LowerHorizontalRegion(node *Node, virtual_space *VirtualSpace)
{
    ASSERT(Node);

    region *Region = &Node->Region;
    region Result;

    Result.X = Region->X;
    Result.Y = Region->Y + (Region->Height * Node->Ratio);
    Result.Width = Region->Width;
    Result.Height = Region->Height * (1 - Node->Ratio);

    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.Y += (Offset->Gap / 2);
        Result.Height -= (Offset->Gap / 2);
    }

    return Result;
}

void CreateNodeRegion(node *Node, region_type Type, macos_space *Space, virtual_space *VirtualSpace)
{
    ASSERT(Type >= Region_Full && Type <= Region_Lower);

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    switch (Type) {
    case Region_Full:   { Node->Region = FullscreenRegion(DisplayRef, VirtualSpace);        } break;
    case Region_Left:   { Node->Region = LeftVerticalRegion(Node->Parent, VirtualSpace);    } break;
    case Region_Right:  { Node->Region = RightVerticalRegion(Node->Parent, VirtualSpace);   } break;
    case Region_Upper:  { Node->Region = UpperHorizontalRegion(Node->Parent, VirtualSpace); } break;
    case Region_Lower:  { Node->Region = LowerHorizontalRegion(Node->Parent, VirtualSpace); } break;
    default:            { /* NOTE(koekeishiya): Invalid region specified. */                } break;
    }

    Node->Region.Type = Type;
    CFRelease(DisplayRef);
}

void CreatePreselectRegion(preselect_node *Preselect, region_type Type, macos_space *Space, virtual_space *VirtualSpace)
{
    ASSERT(Type >= Region_Full && Type <= Region_Lower);

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    switch (Type) {
    case Region_Full:   { Preselect->Region = FullscreenRegion(DisplayRef, VirtualSpace);           } break;
    case Region_Left:   { Preselect->Region = LeftVerticalRegion(Preselect->Node, VirtualSpace);    } break;
    case Region_Right:  { Preselect->Region = RightVerticalRegion(Preselect->Node, VirtualSpace);   } break;
    case Region_Upper:  { Preselect->Region = UpperHorizontalRegion(Preselect->Node, VirtualSpace); } break;
    case Region_Lower:  { Preselect->Region = LowerHorizontalRegion(Preselect->Node, VirtualSpace); } break;
    default:            { /* NOTE(koekeishiya): Invalid region specified. */                        } break;
    }

    Preselect->Region.Type = Type;
    CFRelease(DisplayRef);
}

internal void
CreateNodeRegionPair(node *Left, node *Right, node_split Split, macos_space *Space, virtual_space *VirtualSpace)
{
    ASSERT(Split == Split_Vertical || Split == Split_Horizontal);
    if (Split == Split_Vertical) {
        CreateNodeRegion(Left, Region_Left, Space, VirtualSpace);
        CreateNodeRegion(Right, Region_Right, Space, VirtualSpace);
    } else if (Split == Split_Horizontal) {
        CreateNodeRegion(Left, Region_Upper, Space, VirtualSpace);
        CreateNodeRegion(Right, Region_Lower, Space, VirtualSpace);
    }
}

void ResizeNodeRegion(node *Node, macos_space *Space, virtual_space *VirtualSpace)
{
    if (Node && Node->Left && Node->Right) {
        CreateNodeRegion(Node->Left, Node->Left->Region.Type, Space, VirtualSpace);
        ResizeNodeRegion(Node->Left, Space, VirtualSpace);

        CreateNodeRegion(Node->Right, Node->Right->Region.Type, Space, VirtualSpace);
        ResizeNodeRegion(Node->Right, Space, VirtualSpace);
    }
}

void CreateNodeRegionRecursive(node *Node, bool Optimal, macos_space *Space, virtual_space *VirtualSpace)
{
    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        if (Node && Node->Left && Node->Right) {
            Node->Split = Optimal ? OptimalSplitMode(Node) : Node->Split;
            CreateNodeRegionPair(Node->Left, Node->Right, Node->Split, Space, VirtualSpace);

            CreateNodeRegionRecursive(Node->Left, Optimal, Space, VirtualSpace);
            CreateNodeRegionRecursive(Node->Right, Optimal, Space, VirtualSpace);
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        if (Node && Node->Right) {
            CreateNodeRegion(Node->Right, Region_Full, Space, VirtualSpace);
            CreateNodeRegionRecursive(Node->Right, Optimal, Space, VirtualSpace);
        }
    }
}
