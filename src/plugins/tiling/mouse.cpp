#include "../../common/dispatch/cgeventtap.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/border/border.h"
#include "../../common/misc/assert.h"

#include "node.h"
#include "vspace.h"
#include "controller.h"

#include <string.h>
#include <math.h>

#include <vector>

#define internal static
#define local_persist static

extern macos_window *GetWindowByID(uint32_t Id);

struct resize_border_state
{
    node *Horizontal;
    node *Vertical;
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
internal bool DragResizeActive;

internal resize_border
CreateResizeBorder(node *Node)
{
    // TODO(koekeishiya): read border settings from cvars
    border_window *Border = CreateBorderWindow(Node->Region.X, Node->Region.Y,
                                               Node->Region.Width, Node->Region.Height,
                                               4, 4, 0xffffff00);
    return (resize_border) { Border, Node };
}

internal void
CreateResizeBorders(node *Node)
{
    if(Node->WindowId && Node->WindowId != Node_PseudoLeaf)
    {
        ResizeBorders.push_back(CreateResizeBorder(Node));
    }

    if(Node->Left)
    {
        CreateResizeBorders(Node->Left);
    }

    if(Node->Right)
    {
        CreateResizeBorders(Node->Right);
    }
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
RightMouseDown()
{
    CGPoint Cursor = AXLibGetCursorPos();

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        AXLibDestroySpace(Space);
        return;
    }

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode != Virtual_Space_Bsp)
    {
        ReleaseVirtualSpace(VirtualSpace);
        AXLibDestroySpace(Space);
        return;
    }

    node *Root = VirtualSpace->Tree;
    if(!Root)
    {
        ReleaseVirtualSpace(VirtualSpace);
        AXLibDestroySpace(Space);
        return;
    }

    node *NodeBelowCursor = GetNodeForPoint(Root, &Cursor);
    if(!NodeBelowCursor)
    {
        ReleaseVirtualSpace(VirtualSpace);
        AXLibDestroySpace(Space);
        return;
    }

    macos_window *WindowBelowCursor = GetWindowByID(NodeBelowCursor->WindowId);
    if(!WindowBelowCursor)
    {
        ReleaseVirtualSpace(VirtualSpace);
        AXLibDestroySpace(Space);
        return;
    }

    DragResizeActive = true;
    ResizeState.Space = Space;
    ResizeState.VirtualSpace = VirtualSpace;

    macos_window *NorthWindow, *EastWindow, *SouthWindow, *WestWindow;
    bool North = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &NorthWindow, "north", false);
    bool East = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &EastWindow, "east", false);
    bool South = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &SouthWindow, "south", false);
    bool West = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &WestWindow, "west", false);

    macos_window *VerticalWindow;
    if     (North && South) VerticalWindow = NorthWindow;
    else if(North)          VerticalWindow = NorthWindow;
    else if(South)          VerticalWindow = SouthWindow;
    else                    VerticalWindow = NULL;

    macos_window *HorizontalWindow;
    if     (East && West) HorizontalWindow = EastWindow;
    else if(East)         HorizontalWindow = EastWindow;
    else if(West)         HorizontalWindow = WestWindow;
    else                  HorizontalWindow = NULL;

    if(VerticalWindow)
    {
        node *VerticalNode = GetNodeWithId(Root, VerticalWindow->Id, VirtualSpace->Mode);
        ASSERT(VerticalNode);
        ResizeState.Vertical = GetLowestCommonAncestor(NodeBelowCursor, VerticalNode);
    }

    if(HorizontalWindow)
    {
        node *HorizontalNode = GetNodeWithId(Root, HorizontalWindow->Id, VirtualSpace->Mode);
        ASSERT(HorizontalNode);
        ResizeState.Horizontal = GetLowestCommonAncestor(NodeBelowCursor, HorizontalNode);
    }

    node *Ancestor;
    if     ((ResizeState.Vertical) &&
            (ResizeState.Horizontal))    Ancestor = GetLowestCommonAncestor(ResizeState.Vertical, ResizeState.Horizontal);
    else if(ResizeState.Vertical)        Ancestor = ResizeState.Vertical;
    else if(ResizeState.Horizontal)      Ancestor = ResizeState.Horizontal;
    else                                 Ancestor = Root;

    CreateResizeBorders(Ancestor);
}

internal void
RightMouseDragged()
{
    if(DragResizeActive)
    {
        CGPoint Cursor = AXLibGetCursorPos();
        local_persist float RatioMinDiff = 0.002f;

        if(ResizeState.Vertical)
        {
            float Top = ResizeState.Vertical->Region.Y;
            float Height = ResizeState.Vertical->Region.Height;
            float Ratio = (Cursor.y - Top) / Height;
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
            float Ratio = (Cursor.x - Left) / Width;
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
    if(DragResizeActive)
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
        DragResizeActive = false;
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
        case kCGEventRightMouseDown:
        {
            CGEventFlags Flags = CGEventGetFlags(Event);
            if(Flags & Event_Mask_Alt)
            {
                RightMouseDown();
            }
        } break;
        case kCGEventRightMouseDragged:
        {
            CGEventFlags Flags = CGEventGetFlags(Event);
            if(Flags & Event_Mask_Alt)
            {
                RightMouseDragged();
            }
        } break;
        case kCGEventRightMouseUp:
        {
            CGEventFlags Flags = CGEventGetFlags(Event);
            if(Flags & Event_Mask_Alt)
            {
                RightMouseUp();
            }
        } break;
        default: {} break;
    }

    return Event;
}
