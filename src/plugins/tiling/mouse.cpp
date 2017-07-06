#include "../../common/dispatch/cgeventtap.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/border/border.h"
#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include "node.h"
#include "vspace.h"
#include "controller.h"
#include "constants.h"

#include <string.h>
#include <math.h>

#include <vector>

#define internal static
#define local_persist static

extern macos_window *GetWindowByID(uint32_t Id);

enum drag_mode
{
    Drag_Mode_None = 0,
    Drag_Mode_Swap,
    Drag_Mode_Resize
};

struct resize_border_state
{
    drag_mode Mode;
    node *Horizontal;
    node *Vertical;
    float InitialRatioH;
    float InitialRatioV;
    CGPoint InitialCursor;
    macos_space *Space;
    virtual_space *VirtualSpace;
};

struct resize_border
{
    border_window *Border;
    node *Node;
};

internal std::vector<resize_border> ResizeBorders;
internal resize_border_state ResizeState;

internal resize_border
CreateResizeBorder(node *Node)
{
    unsigned PreselectBorderColor = CVarUnsignedValue(CVAR_PRE_BORDER_COLOR);
    int PreselectBorderWidth = CVarIntegerValue(CVAR_PRE_BORDER_WIDTH);
    int PreselectBorderRadius = CVarIntegerValue(CVAR_PRE_BORDER_RADIUS);
    border_window *Border = CreateBorderWindow(Node->Region.X, Node->Region.Y,
                                               Node->Region.Width, Node->Region.Height,
                                               PreselectBorderWidth, PreselectBorderRadius,
                                               PreselectBorderColor, false);
    return (resize_border) { Border, Node };
}

internal void
CreateResizeBorders(node *Node)
{
    if((Node->WindowId) && (Node->WindowId != Node_PseudoLeaf))
    {
        ResizeBorders.push_back(CreateResizeBorder(Node));
    }

    if(Node->Left)  CreateResizeBorders(Node->Left);
    if(Node->Right) CreateResizeBorders(Node->Right);
}

internal void
UpdateResizeBorders()
{
    std::vector<resize_border>::iterator It;
    for(It = ResizeBorders.begin();
        It != ResizeBorders.end();
        ++It)
    {
        UpdateBorderWindowRect(It->Border,
                               It->Node->Region.X,
                               It->Node->Region.Y,
                               It->Node->Region.Width,
                               It->Node->Region.Height);
    }
}

internal void
FreeResizeBorders()
{
    while(!ResizeBorders.empty())
    {
        border_window *Border = ResizeBorders.back().Border;
        ResizeBorders.pop_back();
        DestroyBorderWindow(Border);
    }
}

internal void
LeftMouseDown()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    virtual_space *VirtualSpace;
    node *Root, *NodeBelowCursor;
    CGPoint Cursor;

    if(Space->Type != kCGSSpaceUser) goto free_space;
    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode != Virtual_Space_Bsp) goto release_vspace;
    if(!(Root = VirtualSpace->Tree)) goto release_vspace;
    Cursor = AXLibGetCursorPos();
    if(!(NodeBelowCursor = GetNodeForPoint(Root, &Cursor))) goto release_vspace;

    ResizeState.Mode = Drag_Mode_Swap;
    ResizeState.Horizontal = NodeBelowCursor;
    ResizeState.Space = Space;
    ResizeState.VirtualSpace = VirtualSpace;

    ResizeBorders.push_back(CreateResizeBorder(NodeBelowCursor));
    goto out;

release_vspace:
        ReleaseVirtualSpace(VirtualSpace);
free_space:
        AXLibDestroySpace(Space);
out:;
}

internal void
LeftMouseDragged()
{
    if(ResizeState.Mode == Drag_Mode_Swap)
    {
        CGPoint Cursor = AXLibGetCursorPos();
        node *NewNode = GetNodeForPoint(ResizeState.VirtualSpace->Tree, &Cursor);
        if(NewNode && NewNode != ResizeState.Vertical)
        {
            ResizeState.Vertical = NewNode;
            if(ResizeBorders.size() == 2)
            {
                border_window *Border = ResizeBorders.back().Border;
                UpdateBorderWindowRect(Border,
                                       NewNode->Region.X,
                                       NewNode->Region.Y,
                                       NewNode->Region.Width,
                                       NewNode->Region.Height);
            }
            else
            {
                ResizeBorders.push_back(CreateResizeBorder(NewNode));
            }
        }
    }
}

internal void
LeftMouseUp()
{
    if(ResizeState.Mode == Drag_Mode_Swap)
    {
        FreeResizeBorders();

        if((ResizeState.Horizontal && ResizeState.Vertical) &&
           (ResizeState.Horizontal != ResizeState.Vertical))
        {
            SwapNodeIds(ResizeState.Horizontal, ResizeState.Vertical);
            ResizeWindowToRegionSize(ResizeState.Horizontal);
            ResizeWindowToRegionSize(ResizeState.Vertical);
        }

        if(ResizeState.Space && ResizeState.VirtualSpace)
        {
            ReleaseVirtualSpace(ResizeState.VirtualSpace);
            AXLibDestroySpace(ResizeState.Space);
        }

        memset(&ResizeState, 0, sizeof(resize_border_state));
    }
}

internal void
RightMouseDown()
{
    CGPoint Cursor = AXLibGetCursorPos();

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    virtual_space *VirtualSpace;
    bool North, East, South, West;
    macos_window *WindowBelowCursor;
    node *Root, *NodeBelowCursor, *Ancestor;
    macos_window *VerticalWindow, *HorizontalWindow;
    macos_window *NorthWindow, *EastWindow, *SouthWindow, *WestWindow;

    if(Space->Type != kCGSSpaceUser) goto free_space;
    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode != Virtual_Space_Bsp) goto release_vspace;
    if(!(Root = VirtualSpace->Tree)) goto release_vspace;
    if(!(NodeBelowCursor = GetNodeForPoint(Root, &Cursor))) goto release_vspace;
    if(!(WindowBelowCursor = GetWindowByID(NodeBelowCursor->WindowId))) goto release_vspace;

    ResizeState.Mode = Drag_Mode_Resize;
    ResizeState.InitialCursor = Cursor;
    ResizeState.Space = Space;
    ResizeState.VirtualSpace = VirtualSpace;

    North = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &NorthWindow, "north", false);
    East = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &EastWindow, "east", false);
    South = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &SouthWindow, "south", false);
    West = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &WestWindow, "west", false);

    if     (North && South) VerticalWindow = NorthWindow;
    else if(North)          VerticalWindow = NorthWindow;
    else if(South)          VerticalWindow = SouthWindow;
    else                    VerticalWindow = NULL;

    if     (East && West) HorizontalWindow = EastWindow;
    else if(East)         HorizontalWindow = EastWindow;
    else if(West)         HorizontalWindow = WestWindow;
    else                  HorizontalWindow = NULL;

    if(VerticalWindow)
    {
        node *VerticalNode = GetNodeWithId(Root, VerticalWindow->Id, VirtualSpace->Mode);
        ASSERT(VerticalNode);
        ResizeState.Vertical = GetLowestCommonAncestor(NodeBelowCursor, VerticalNode);
        ResizeState.InitialRatioV = ResizeState.Vertical->Ratio;
    }

    if(HorizontalWindow)
    {
        node *HorizontalNode = GetNodeWithId(Root, HorizontalWindow->Id, VirtualSpace->Mode);
        ASSERT(HorizontalNode);
        ResizeState.Horizontal = GetLowestCommonAncestor(NodeBelowCursor, HorizontalNode);
        ResizeState.InitialRatioH = ResizeState.Horizontal->Ratio;
    }

    if     ((ResizeState.Vertical) &&
            (ResizeState.Horizontal))    Ancestor = GetLowestCommonAncestor(ResizeState.Vertical, ResizeState.Horizontal);
    else if(ResizeState.Vertical)        Ancestor = ResizeState.Vertical;
    else if(ResizeState.Horizontal)      Ancestor = ResizeState.Horizontal;
    else                                 Ancestor = Root;

    CreateResizeBorders(Ancestor);
    goto out;

release_vspace:
    ReleaseVirtualSpace(VirtualSpace);
free_space:
    AXLibDestroySpace(Space);
out:;
}

internal void
RightMouseDragged()
{
    if(ResizeState.Mode == Drag_Mode_Resize)
    {
        CGPoint Cursor = AXLibGetCursorPos();
        local_persist float RatioMinDiff = 0.002f;

        if(ResizeState.Vertical)
        {
            float Top = ResizeState.Vertical->Region.Y;
            float Height = ResizeState.Vertical->Region.Height;

            float InitialCursorWindowYPos = ResizeState.InitialCursor.y - Top;
            float CursorWindowYPos = Cursor.y - Top;

            float RegionCenterY = Height * ResizeState.InitialRatioV;
            float DeltaY = RegionCenterY - InitialCursorWindowYPos;

            float Ratio = (CursorWindowYPos + DeltaY) / Height;
            if((fabs(Ratio - ResizeState.Vertical->Ratio) > RatioMinDiff) &&
               (Ratio >= 0.1f && Ratio <= 0.9f))
            {
                ResizeState.Vertical->Ratio = Ratio;
                ResizeNodeRegion(ResizeState.Vertical, ResizeState.Space, ResizeState.VirtualSpace);

                /* NOTE(koekeishiya): Enable to perform live resizing !!!
                ApplyNodeRegion(ResizeState.Vertical, ResizeState.VirtualSpace->Mode);
                */
            }
        }

        if(ResizeState.Horizontal)
        {
            float Left = ResizeState.Horizontal->Region.X;
            float Width = ResizeState.Horizontal->Region.Width;

            float InitialCursorWindowXPos = ResizeState.InitialCursor.x - Left;
            float CursorWindowXPos = Cursor.x - Left;

            float RegionCenterX = Width * ResizeState.InitialRatioH;
            float DeltaX = RegionCenterX - InitialCursorWindowXPos;

            float Ratio = (CursorWindowXPos + DeltaX) / Width;
            if((fabs(Ratio - ResizeState.Horizontal->Ratio) > RatioMinDiff) &&
               (Ratio >= 0.1f && Ratio <= 0.9f))
            {
                ResizeState.Horizontal->Ratio = Ratio;
                ResizeNodeRegion(ResizeState.Horizontal, ResizeState.Space, ResizeState.VirtualSpace);

                /* NOTE(koekeishiya): Enable to perform live resizing !!!
                ApplyNodeRegion(ResizeState.Horizontal, ResizeState.VirtualSpace->Mode);
                */
            }
        }

        UpdateResizeBorders();
    }
}

internal void
RightMouseUp()
{
    if(ResizeState.Mode == Drag_Mode_Resize)
    {
        FreeResizeBorders();

        if(ResizeState.Horizontal)
        {
            ApplyNodeRegion(ResizeState.Horizontal, ResizeState.VirtualSpace->Mode, true);
        }

        if(ResizeState.Vertical)
        {
            ApplyNodeRegion(ResizeState.Vertical, ResizeState.VirtualSpace->Mode, true);
        }

        ReleaseVirtualSpace(ResizeState.VirtualSpace);
        AXLibDestroySpace(ResizeState.Space);

        memset(&ResizeState, 0, sizeof(resize_border_state));
    }
}

EVENTTAP_CALLBACK(EventTapCallback)
{
    event_tap *EventTap = (event_tap *) Reference;
    switch(Type)
    {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
        {
            CGEventTapEnable(EventTap->Handle, true);
        } break;
        case kCGEventLeftMouseDown:
        {
            CGEventFlags Flags = CGEventGetFlags(Event);
            if((Flags & Event_Mask_Fn) &&
               (ResizeState.Mode == Drag_Mode_None))
            {
                LeftMouseDown();
                return NULL;
            }
        } break;
        case kCGEventLeftMouseDragged:
        {
            LeftMouseDragged();
        } break;
        case kCGEventLeftMouseUp:
        {
            LeftMouseUp();
        } break;
        case kCGEventRightMouseDown:
        {
            CGEventFlags Flags = CGEventGetFlags(Event);
            if((Flags & Event_Mask_Fn) &&
               (ResizeState.Mode == Drag_Mode_None))
            {
                RightMouseDown();
                return NULL;
            }
        } break;
        case kCGEventRightMouseDragged:
        {
            RightMouseDragged();
        } break;
        case kCGEventRightMouseUp:
        {
            RightMouseUp();
        } break;
        default: {} break;
    }

    return Event;
}
