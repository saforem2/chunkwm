#include "../../common/dispatch/cgeventtap.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/border/border.h"
#include "../../common/config/cvar.h"
#include "../../common/config/tokenize.h"
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

extern "C" OSStatus CGSFindWindowByGeometry(int cid, int zero, int one, int zero_again, CGPoint *screen_point, CGPoint *window_coords_out, int *wid_out, int *cid_out);
extern macos_window *GetWindowByID(uint32_t Id);

enum drag_mode
{
    Drag_Mode_None = 0,
    Drag_Mode_Swap,
    Drag_Mode_Resize,
    Drag_Mode_Move_Floating,
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
    macos_window *Window;
};

struct resize_border
{
    border_window *Border;
    node *Node;
};

internal std::vector<resize_border> ResizeBorders;
internal resize_border_state ResizeState;
internal uint32_t MouseModifier;

internal resize_border
CreateResizeBorder(node *Node)
{
    unsigned PreselectBorderColor = CVarUnsignedValue(CVAR_PRE_BORDER_COLOR);
    int PreselectBorderWidth = CVarIntegerValue(CVAR_PRE_BORDER_WIDTH);
    int PreselectBorderRadius = CVarIntegerValue(CVAR_PRE_BORDER_RADIUS);
    int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(Node->Region.Y, Node->Region.Height);
    border_window *Border = CreateBorderWindow(Node->Region.X, InvertY,
                                               Node->Region.Width, Node->Region.Height,
                                               PreselectBorderWidth, PreselectBorderRadius,
                                               PreselectBorderColor);
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
        int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(It->Node->Region.Y, It->Node->Region.Height);
        UpdateBorderWindowRect(It->Border,
                               It->Node->Region.X,
                               InvertY,
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

internal bool
BeginFloatingWindow(virtual_space *VirtualSpace)
{
    macos_window *Window;
    CGPoint Cursor;
    int WindowId;
    int WindowConnection;
    CGPoint WindowPosition;

    local_persist int Connection = _CGSDefaultConnection();
    Cursor = AXLibGetCursorPos();
    WindowId = 0;

    CGSFindWindowByGeometry(Connection, 0, 1, 0, &Cursor, &WindowPosition, &WindowId, &WindowConnection);
    if((Connection != WindowConnection) &&
       (WindowId > 0))
    {
        Window = GetWindowByID(WindowId);
    }
    else
    {
        Window =  GetFocusedWindow();
    }

    if((VirtualSpace->Mode == Virtual_Space_Float) ||
       (AXLibHasFlags(Window, Window_Float)))
    {
        if((Cursor.x >= Window->Position.x) &&
           (Cursor.x <= Window->Position.x + Window->Size.width) &&
           (Cursor.y >= Window->Position.y) &&
           (Cursor.y <= Window->Position.y + Window->Size.height))
        {
            ResizeState.Window = Window;
            ResizeState.InitialCursor = Cursor;
            ResizeState.Mode = Drag_Mode_Move_Floating;
            ResizeState.InitialRatioH = Window->Position.x;
            ResizeState.InitialRatioV = Window->Position.y;
            return true;
        }
    }

    return false;
}

internal bool
BeginTiledWindow(macos_space *Space, virtual_space *VirtualSpace)
{
    node *Root, *NodeBelowCursor;
    CGPoint Cursor;

    if(VirtualSpace->Mode != Virtual_Space_Bsp) return false;
    if(!(Root = VirtualSpace->Tree)) return false;

    Cursor = AXLibGetCursorPos();
    if(!(NodeBelowCursor = GetNodeForPoint(Root, &Cursor))) return false;

    ResizeState.Mode = Drag_Mode_Swap;
    ResizeState.Horizontal = NodeBelowCursor;
    ResizeState.Space = Space;
    ResizeState.VirtualSpace = VirtualSpace;

    ResizeBorders.push_back(CreateResizeBorder(NodeBelowCursor));
    return true;
}

internal void
LeftMouseDown()
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser) goto free_space;
    VirtualSpace = AcquireVirtualSpace(Space);

    if(BeginFloatingWindow(VirtualSpace))
    {
        goto release_vspace;
    }
    else if(BeginTiledWindow(Space, VirtualSpace))
    {
        goto out;
    }

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
                int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(NewNode->Region.Y, NewNode->Region.Height);
                UpdateBorderWindowRect(Border,
                                       NewNode->Region.X,
                                       InvertY,
                                       NewNode->Region.Width,
                                       NewNode->Region.Height);
            }
            else
            {
                ResizeBorders.push_back(CreateResizeBorder(NewNode));
            }
        }
    }
    else if(ResizeState.Mode == Drag_Mode_Move_Floating)
    {
        local_persist int UseCGSMove = CVarIntegerValue(CVAR_WINDOW_CGS_MOVE);
        local_persist float MinDiff = 2.5f;
        CGPoint Cursor = AXLibGetCursorPos();
        float DeltaX = Cursor.x - ResizeState.InitialCursor.x;
        float DeltaY = Cursor.y - ResizeState.InitialCursor.y;
        if(fabs(DeltaX) > MinDiff || fabs(DeltaY) > MinDiff)
        {
            if(UseCGSMove)
            {
                ExtendedDockSetWindowPosition(ResizeState.Window->Id,
                                              (int)(ResizeState.InitialRatioH + DeltaX),
                                              (int)(ResizeState.InitialRatioV + DeltaY));
            }
            else
            {
                AXLibSetWindowPosition(ResizeState.Window->Ref,
                                       (int)(ResizeState.InitialRatioH + DeltaX),
                                       (int)(ResizeState.InitialRatioV + DeltaY));
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
    else if(ResizeState.Mode == Drag_Mode_Move_Floating)
    {
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
            if(((Flags & MouseModifier) == MouseModifier) &&
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
            if(((Flags & MouseModifier) == MouseModifier) &&
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

// NOTE(koekeishiya): This function should only be called once (during init) !!
void SetMouseModifier(const char *Mod)
{
    while(*Mod)
    {
        token ModToken = GetToken(&Mod);
        if(TokenEquals(ModToken, "fn"))
        {
            MouseModifier |= Event_Mask_Fn;
        }
        else if(TokenEquals(ModToken, "shift"))
        {
            MouseModifier |= Event_Mask_Shift;
        }
        else if(TokenEquals(ModToken, "alt"))
        {
            MouseModifier |= Event_Mask_Alt;
        }
        else if(TokenEquals(ModToken, "cmd"))
        {
            MouseModifier |= Event_Mask_Cmd;
        }
        else if(TokenEquals(ModToken, "ctrl"))
        {
            MouseModifier |= Event_Mask_Control;
        }
    }

    // NOTE(koekeishiya): If no matches were found, we default to FN
    if(MouseModifier == 0) MouseModifier |= Event_Mask_Fn;
}
