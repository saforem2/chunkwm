#include "mouse.h"

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
    Drag_Mode_Resize_Floating,
};

struct resize_border_state
{
    drag_mode Mode;
    node *Horizontal;
    node *Vertical;
    float InitialRatioH;
    float InitialRatioV;
    float InitialWidth;
    float InitialHeight;
    CGPoint InitialCursor;
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window;
    uint64_t LastEventTime;
};

struct resize_border
{
    border_window *Border;
    node *Node;
    macos_window *Window;
};

enum mouse_binding_flag
{
    Mouse_Binding_Flag_Alt         = (1 << 0),
    Mouse_Binding_Flag_Shift       = (1 << 1),
    Mouse_Binding_Flag_Cmd         = (1 << 2),
    Mouse_Binding_Flag_Control     = (1 << 3),
    Mouse_Binding_Flag_Fn          = (1 << 4),
};
struct mouse_binding
{
    bool Active;
    uint32_t Button;
    uint32_t Flags;
};

internal std::vector<resize_border> ResizeBorders;
internal resize_border_state ResizeState;

internal mouse_binding MouseMove;
internal mouse_binding MouseResize;

internal CGEventRef CurrentEvent;

internal inline bool
IsMouseMoveInProgress()
{
    return ((ResizeState.Mode == Drag_Mode_Swap) ||
            (ResizeState.Mode == Drag_Mode_Move_Floating));
}

internal inline bool
IsMouseResizeInProgress()
{
    return ((ResizeState.Mode == Drag_Mode_Resize) ||
            (ResizeState.Mode == Drag_Mode_Resize_Floating));
}

internal inline bool
IsMouseActionInProgress()
{
    return (ResizeState.Mode != Drag_Mode_None);
}

internal inline uint32_t
CgEventFlagsToMouseBindingFlags(uint32_t Flags)
{
    uint32_t MouseBindingFlags = 0;
    if ((Flags & Event_Mask_Fn)      == Event_Mask_Fn)      { MouseBindingFlags |= Mouse_Binding_Flag_Fn;      }
    if ((Flags & Event_Mask_Shift)   == Event_Mask_Shift)   { MouseBindingFlags |= Mouse_Binding_Flag_Shift;   }
    if ((Flags & Event_Mask_Alt)     == Event_Mask_Alt)     { MouseBindingFlags |= Mouse_Binding_Flag_Alt;     }
    if ((Flags & Event_Mask_Cmd)     == Event_Mask_Cmd)     { MouseBindingFlags |= Mouse_Binding_Flag_Cmd;     }
    if ((Flags & Event_Mask_Control) == Event_Mask_Control) { MouseBindingFlags |= Mouse_Binding_Flag_Control; }
    return MouseBindingFlags;
}

internal inline bool
MatchMouseBinding(mouse_binding *Binding, uint32_t Flags, uint32_t Button)
{
    bool Result = ((Binding->Active) &&
                   (Binding->Flags == Flags) &&
                   (Binding->Button == Button));
    return Result;
}

internal resize_border
CreateResizeBorder(node *Node, macos_window *Window)
{
    unsigned PreselectBorderColor = CVarUnsignedValue(CVAR_PRE_BORDER_COLOR);
    int PreselectBorderWidth = CVarIntegerValue(CVAR_PRE_BORDER_WIDTH);
    int PreselectBorderRadius = CVarIntegerValue(CVAR_PRE_BORDER_RADIUS);
    int PreselectBorderOutline = CVarIntegerValue(CVAR_PRE_BORDER_OUTLINE);
    region PreselRegion = RoundPreselRegion(Node->Region, Window->Position, Window->Size);
    int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(PreselRegion.Y, PreselRegion.Height);
    border_window *Border = CreateBorderWindow(PreselRegion.X, InvertY,
                                               PreselRegion.Width, PreselRegion.Height,
                                               PreselectBorderWidth, PreselectBorderRadius,
                                               PreselectBorderColor, PreselectBorderOutline);
    return (resize_border) { Border, Node, Window };
}

internal void
CreateResizeBorders(node *Node)
{
    if ((Node->WindowId) && (Node->WindowId != Node_PseudoLeaf)) {
        macos_window *Window = GetWindowByID(Node->WindowId);
        ASSERT(Window);
        ResizeBorders.push_back(CreateResizeBorder(Node, Window));
    }

    if (Node->Left)  CreateResizeBorders(Node->Left);
    if (Node->Right) CreateResizeBorders(Node->Right);
}

internal void
UpdateResizeBorders()
{
    std::vector<resize_border>::iterator It;
    for (It = ResizeBorders.begin(); It != ResizeBorders.end(); ++It) {
        region PreselRegion = RoundPreselRegion(It->Node->Region, It->Window->Position, It->Window->Size);
        int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(PreselRegion.Y, PreselRegion.Height);
        UpdateBorderWindowRect(It->Border,
                               PreselRegion.X,
                               InvertY,
                               PreselRegion.Width,
                               PreselRegion.Height);
    }
}

internal void
FreeResizeBorders()
{
    while (!ResizeBorders.empty()) {
        border_window *Border = ResizeBorders.back().Border;
        ResizeBorders.pop_back();
        DestroyBorderWindow(Border);
    }
}

internal bool
BeginFloatingWindow(virtual_space *VirtualSpace, drag_mode DragMode)
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
    if ((Connection != WindowConnection) && (WindowId > 0)) {
        Window = GetWindowByID(WindowId);
    } else {
        Window =  GetFocusedWindow();
    }

    if (!Window) {
        /*
         * NOTE(koekeishiya): We can potentially get a null-pointer returned here, because
         * the window is not a valid window in the eyes of chunkwm, for whatever reason.
         *
         * This can be reproduced by having Übersicht draw a window and try to initiate
         * mouse-drag.
         *
         */

        return false;
    }

    if ((VirtualSpace->Mode == Virtual_Space_Float) || (AXLibHasFlags(Window, Window_Float))) {
        if ((Cursor.x >= Window->Position.x) &&
            (Cursor.x <= Window->Position.x + Window->Size.width) &&
            (Cursor.y >= Window->Position.y) &&
            (Cursor.y <= Window->Position.y + Window->Size.height)) {
            ResizeState.Window = Window;
            ResizeState.InitialCursor = Cursor;
            ResizeState.Mode = DragMode;
            ResizeState.InitialRatioH = Window->Position.x;
            ResizeState.InitialRatioV = Window->Position.y;
            ResizeState.InitialWidth = Window->Size.width;
            ResizeState.InitialHeight = Window->Size.height;
            ResizeState.LastEventTime = CGEventGetTimestamp(CurrentEvent);
            return true;
        }
    }

    return false;
}

internal bool
BeginTiledWindow(macos_space *Space, virtual_space *VirtualSpace, drag_mode DragMode)
{
    macos_window *WindowBelowCursor;
    node *Root, *NodeBelowCursor;
    CGPoint Cursor = AXLibGetCursorPos();

    if (VirtualSpace->Mode != Virtual_Space_Bsp) return false;
    if (!(Root = VirtualSpace->Tree)) return false;
    if (!(NodeBelowCursor = GetNodeForPoint(Root, &Cursor))) return false;
    if (!(WindowBelowCursor = GetWindowByID(NodeBelowCursor->WindowId))) return false;

    if (DragMode == Drag_Mode_Swap) {
        ResizeState.Mode = Drag_Mode_Swap;
        ResizeState.Horizontal = NodeBelowCursor;
        ResizeState.Space = Space;
        ResizeState.VirtualSpace = VirtualSpace;
        ResizeState.Window = WindowBelowCursor;
        ResizeBorders.push_back(CreateResizeBorder(NodeBelowCursor, WindowBelowCursor));
        return true;
    } else if (DragMode == Drag_Mode_Resize) {
        node *Ancestor;
        bool North, East, South, West;
        macos_window *VerticalWindow, *HorizontalWindow;
        macos_window *NorthWindow, *EastWindow, *SouthWindow, *WestWindow;

        ResizeState.Mode = Drag_Mode_Resize;
        ResizeState.InitialCursor = Cursor;
        ResizeState.Space = Space;
        ResizeState.VirtualSpace = VirtualSpace;

        North = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &NorthWindow, "north", false);
        East  = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &EastWindow, "east", false);
        South = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &SouthWindow, "south", false);
        West  = FindClosestWindow(Space, VirtualSpace, WindowBelowCursor, &WestWindow, "west", false);

        if      (North && South) VerticalWindow = NorthWindow;
        else if (North)          VerticalWindow = NorthWindow;
        else if (South)          VerticalWindow = SouthWindow;
        else                     VerticalWindow = NULL;

        if      (East && West) HorizontalWindow = EastWindow;
        else if (East)         HorizontalWindow = EastWindow;
        else if (West)         HorizontalWindow = WestWindow;
        else                   HorizontalWindow = NULL;

        if (VerticalWindow) {
            node *VerticalNode = GetNodeWithId(Root, VerticalWindow->Id, VirtualSpace->Mode);
            ASSERT(VerticalNode);
            ResizeState.Vertical = GetLowestCommonAncestor(NodeBelowCursor, VerticalNode);
            ResizeState.InitialRatioV = ResizeState.Vertical->Ratio;
        }

        if (HorizontalWindow) {
            node *HorizontalNode = GetNodeWithId(Root, HorizontalWindow->Id, VirtualSpace->Mode);
            ASSERT(HorizontalNode);
            ResizeState.Horizontal = GetLowestCommonAncestor(NodeBelowCursor, HorizontalNode);
            ResizeState.InitialRatioH = ResizeState.Horizontal->Ratio;
        }

        if      ((ResizeState.Vertical) &&
                 (ResizeState.Horizontal))    Ancestor = GetLowestCommonAncestor(ResizeState.Vertical, ResizeState.Horizontal);
        else if (ResizeState.Vertical)        Ancestor = ResizeState.Vertical;
        else if (ResizeState.Horizontal)      Ancestor = ResizeState.Horizontal;
        else                                  Ancestor = Root;

        ResizeState.LastEventTime = CGEventGetTimestamp(CurrentEvent);

        CreateResizeBorders(Ancestor);
        return true;
    }

    return false;
}

internal void
MouseMoveWindowBegin()
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if (Space->Type != kCGSSpaceUser) goto free_space;
    VirtualSpace = AcquireVirtualSpace(Space);

    if (BeginFloatingWindow(VirtualSpace, Drag_Mode_Move_Floating)) {
        goto release_vspace;
    } else if (BeginTiledWindow(Space, VirtualSpace, Drag_Mode_Swap)) {
        goto out;
    }

release_vspace:
    ReleaseVirtualSpace(VirtualSpace);
free_space:
    AXLibDestroySpace(Space);
out:;
}

internal void
MouseMoveWindowTick()
{
    if (ResizeState.Mode == Drag_Mode_Swap) {
        CGPoint Cursor = AXLibGetCursorPos();
        node *NewNode = GetNodeForPoint(ResizeState.VirtualSpace->Tree, &Cursor);
        if (NewNode && NewNode != ResizeState.Vertical) {
            ResizeState.Vertical = NewNode;
            if (ResizeBorders.size() == 2) {
                resize_border ResizeBorder = ResizeBorders.back();
                border_window *Border = ResizeBorder.Border;
                macos_window *Window = ResizeBorder.Window;
                region PreselRegion = RoundPreselRegion(NewNode->Region, Window->Position, Window->Size);
                int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(PreselRegion.Y, PreselRegion.Height);
                UpdateBorderWindowRect(Border,
                                       PreselRegion.X,
                                       InvertY,
                                       PreselRegion.Width,
                                       PreselRegion.Height);
            } else {
                macos_window *Window = GetWindowByID(NewNode->WindowId);
                ASSERT(Window);
                ResizeBorders.push_back(CreateResizeBorder(NewNode, Window));
            }
        }
    } else if (ResizeState.Mode == Drag_Mode_Move_Floating) {
        local_persist int UseCGSMove = CVarIntegerValue(CVAR_WINDOW_CGS_MOVE);
        local_persist float MinDiff = 2.5f;
        CGPoint Cursor = AXLibGetCursorPos();
        float DeltaX = Cursor.x - ResizeState.InitialCursor.x;
        float DeltaY = Cursor.y - ResizeState.InitialCursor.y;
        if (fabs(DeltaX) > MinDiff || fabs(DeltaY) > MinDiff) {
            if (UseCGSMove) {
                ExtendedDockSetWindowPosition(ResizeState.Window->Id,
                                              (int)(ResizeState.InitialRatioH + DeltaX),
                                              (int)(ResizeState.InitialRatioV + DeltaY));
            } else {
                float MouseMotionInterval = CVarFloatingPointValue(CVAR_MOUSE_MOTION_INTERVAL);
                uint64_t CurrentEventTime = CGEventGetTimestamp(CurrentEvent);
                float DeltaEventTime = ((float)CurrentEventTime - ResizeState.LastEventTime) * (1.0f / 1E6);

                if (DeltaEventTime < MouseMotionInterval) {
                    return;
                }

                ResizeState.LastEventTime = CurrentEventTime;

                AXLibSetWindowPosition(ResizeState.Window->Ref,
                                       (int)(ResizeState.InitialRatioH + DeltaX),
                                       (int)(ResizeState.InitialRatioV + DeltaY));
            }
        }
    }
}

internal void
MouseMoveWindowEnd()
{
    if (ResizeState.Mode == Drag_Mode_Swap) {
        FreeResizeBorders();

        if ((ResizeState.Horizontal && ResizeState.Vertical) &&
            (ResizeState.Horizontal != ResizeState.Vertical)) {
            SwapNodeIds(ResizeState.Horizontal, ResizeState.Vertical);
            ResizeWindowToRegionSize(ResizeState.Horizontal);
            ResizeWindowToRegionSize(ResizeState.Vertical);
        }

        if (ResizeState.Space && ResizeState.VirtualSpace) {
            ReleaseVirtualSpace(ResizeState.VirtualSpace);
            AXLibDestroySpace(ResizeState.Space);
        }

        memset(&ResizeState, 0, sizeof(resize_border_state));
    } else if (ResizeState.Mode == Drag_Mode_Move_Floating) {
        memset(&ResizeState, 0, sizeof(resize_border_state));
    }
}

internal void
MouseResizeWindowBegin()
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if (Space->Type != kCGSSpaceUser) goto free_space;
    VirtualSpace = AcquireVirtualSpace(Space);

    if (BeginFloatingWindow(VirtualSpace, Drag_Mode_Resize_Floating)) {
        goto release_vspace;
    } else if (BeginTiledWindow(Space, VirtualSpace, Drag_Mode_Resize)) {
        goto out;
    }

release_vspace:
    ReleaseVirtualSpace(VirtualSpace);
free_space:
    AXLibDestroySpace(Space);
out:;
}

internal void
MouseResizeWindowTick()
{
    if (ResizeState.Mode == Drag_Mode_Resize) {
        CGPoint Cursor = AXLibGetCursorPos();
        local_persist float RatioMinDiff = 0.002f;

        if (ResizeState.Vertical) {
            float Top = ResizeState.Vertical->Region.Y;
            float Height = ResizeState.Vertical->Region.Height;

            float InitialCursorWindowYPos = ResizeState.InitialCursor.y - Top;
            float CursorWindowYPos = Cursor.y - Top;

            float RegionCenterY = Height * ResizeState.InitialRatioV;
            float DeltaY = RegionCenterY - InitialCursorWindowYPos;

            float Ratio = (CursorWindowYPos + DeltaY) / Height;
            if ((fabs(Ratio - ResizeState.Vertical->Ratio) > RatioMinDiff) &&
                (Ratio >= 0.1f && Ratio <= 0.9f)) {
                ResizeState.Vertical->Ratio = Ratio;
                ResizeNodeRegion(ResizeState.Vertical, ResizeState.Space, ResizeState.VirtualSpace);
            }
        }

        if (ResizeState.Horizontal) {
            float Left = ResizeState.Horizontal->Region.X;
            float Width = ResizeState.Horizontal->Region.Width;

            float InitialCursorWindowXPos = ResizeState.InitialCursor.x - Left;
            float CursorWindowXPos = Cursor.x - Left;

            float RegionCenterX = Width * ResizeState.InitialRatioH;
            float DeltaX = RegionCenterX - InitialCursorWindowXPos;

            float Ratio = (CursorWindowXPos + DeltaX) / Width;
            if ((fabs(Ratio - ResizeState.Horizontal->Ratio) > RatioMinDiff) &&
                (Ratio >= 0.1f && Ratio <= 0.9f)) {
                ResizeState.Horizontal->Ratio = Ratio;
                ResizeNodeRegion(ResizeState.Horizontal, ResizeState.Space, ResizeState.VirtualSpace);
            }
        }

        UpdateResizeBorders();
    } else if (ResizeState.Mode == Drag_Mode_Resize_Floating) {
        float MouseMotionInterval = CVarFloatingPointValue(CVAR_MOUSE_MOTION_INTERVAL);
        uint64_t CurrentEventTime = CGEventGetTimestamp(CurrentEvent);
        float DeltaEventTime = ((float)CurrentEventTime - ResizeState.LastEventTime) * (1.0f / 1E6);

        if (DeltaEventTime < MouseMotionInterval) {
            return;
        }

        ResizeState.LastEventTime = CurrentEventTime;

        macos_window *Window = ResizeState.Window;

        CGPoint InitialCursor = ResizeState.InitialCursor;
        CGPoint InitialCursorInWindow = {
            InitialCursor.x - ResizeState.InitialRatioH,
            InitialCursor.y - ResizeState.InitialRatioV
        };

        CGPoint Cursor = AXLibGetCursorPos();
        CGPoint CursorInWindow = {
            Cursor.x - ResizeState.InitialRatioH,
            Cursor.y - ResizeState.InitialRatioV
        };

        CGPoint DeltaCursor = {
            CursorInWindow.x - InitialCursorInWindow.x,
            CursorInWindow.y - InitialCursorInWindow.y
        };

        CGPoint WindowCenter = {
            ResizeState.InitialWidth / 2,
            ResizeState.InitialHeight / 2
        };

        ResizeState.InitialCursor = Cursor;

        if (CursorInWindow.x < WindowCenter.x) {
            if (CursorInWindow.y < WindowCenter.y) {
                // NOTE(koekeishiya): The user is gripping the top left corner
                ResizeState.InitialRatioH += DeltaCursor.x;
                ResizeState.InitialRatioV += DeltaCursor.y;
                ResizeState.InitialWidth -= DeltaCursor.x;
                ResizeState.InitialHeight -= DeltaCursor.y;

                AXLibSetWindowPosition(Window->Ref,
                                       ResizeState.InitialRatioH,
                                       ResizeState.InitialRatioV);
                AXLibSetWindowSize(Window->Ref,
                                   ResizeState.InitialWidth,
                                   ResizeState.InitialHeight);
            }

            if (CursorInWindow.y > WindowCenter.y) {
                // NOTE(koekeishiya): The user is gripping the bottom left corner
                ResizeState.InitialRatioH += DeltaCursor.x;
                ResizeState.InitialHeight += DeltaCursor.y;
                ResizeState.InitialWidth -= DeltaCursor.x;

                AXLibSetWindowPosition(Window->Ref,
                                       ResizeState.InitialRatioH,
                                       ResizeState.InitialRatioV);
                AXLibSetWindowSize(Window->Ref,
                                   ResizeState.InitialWidth,
                                   ResizeState.InitialHeight);
            }
        }

        if (CursorInWindow.x > WindowCenter.x) {
            if (CursorInWindow.y < WindowCenter.y) {
                // NOTE(koekeishiya): The user is gripping the top right corner
                ResizeState.InitialRatioV += DeltaCursor.y;
                ResizeState.InitialWidth += DeltaCursor.x;
                ResizeState.InitialHeight -= DeltaCursor.y;

                AXLibSetWindowPosition(Window->Ref,
                                       ResizeState.InitialRatioH,
                                       ResizeState.InitialRatioV);
                AXLibSetWindowSize(Window->Ref,
                                   ResizeState.InitialWidth,
                                   ResizeState.InitialHeight);
            }

            if (CursorInWindow.y > WindowCenter.y) {
                // NOTE(koekeishiya): The user is gripping the bottom right corner
                ResizeState.InitialHeight += DeltaCursor.y;
                ResizeState.InitialWidth += DeltaCursor.x;

                AXLibSetWindowPosition(Window->Ref,
                                       ResizeState.InitialRatioH,
                                       ResizeState.InitialRatioV);
                AXLibSetWindowSize(Window->Ref,
                                   ResizeState.InitialWidth,
                                   ResizeState.InitialHeight);
            }
        }
    }
}

internal void
MouseResizeWindowEnd()
{
    if (ResizeState.Mode == Drag_Mode_Resize) {
        FreeResizeBorders();

        if (ResizeState.Horizontal) {
            ApplyNodeRegion(ResizeState.Horizontal, ResizeState.VirtualSpace->Mode, true);
        }

        if (ResizeState.Vertical) {
            ApplyNodeRegion(ResizeState.Vertical, ResizeState.VirtualSpace->Mode, true);
        }

        ReleaseVirtualSpace(ResizeState.VirtualSpace);
        AXLibDestroySpace(ResizeState.Space);

        memset(&ResizeState, 0, sizeof(resize_border_state));
    } else if (ResizeState.Mode == Drag_Mode_Resize_Floating) {
        memset(&ResizeState, 0, sizeof(resize_border_state));
    }
}

EVENTTAP_CALLBACK(EventTapCallback)
{
    event_tap *EventTap = (event_tap *) Reference;

    CurrentEvent = Event;

    switch (Type) {
    case kCGEventTapDisabledByTimeout:
    case kCGEventTapDisabledByUserInput: {
        CGEventTapEnable(EventTap->Handle, true);
    } break;
    case kCGEventLeftMouseDown:
    case kCGEventRightMouseDown: {
        if (!IsMouseActionInProgress()) {
            uint32_t Flags = CgEventFlagsToMouseBindingFlags(CGEventGetFlags(Event));
            uint32_t Button = CGEventGetIntegerValueField(Event, kCGMouseEventButtonNumber);

            if (MatchMouseBinding(&MouseMove, Flags, Button)) {
                MouseMoveWindowBegin();
                return NULL;
            }

            if (MatchMouseBinding(&MouseResize, Flags, Button)) {
                MouseResizeWindowBegin();
                return NULL;
            }
        }
    } break;
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged: {
        if (IsMouseMoveInProgress()) {
            MouseMoveWindowTick();
        } else if (IsMouseResizeInProgress()) {
            MouseResizeWindowTick();
        }
    } break;
    case kCGEventLeftMouseUp:
    case kCGEventRightMouseUp: {
        if (IsMouseMoveInProgress()) {
            MouseMoveWindowEnd();
        } else if (IsMouseResizeInProgress()) {
            MouseResizeWindowEnd();
        }
    } break;
    default: {} break;
    }

    return Event;
}

internal void
ParseMouseBinding(mouse_binding *MouseBinding, const char *BindSym)
{
    while (*BindSym) {
        token Token = GetToken(&BindSym);
        if (!Token.Text || Token.Length == 0) {
            break;
        }

        if (TokenEquals(Token, "fn")) {
            MouseBinding->Flags |= Mouse_Binding_Flag_Fn;
        } else if (TokenEquals(Token, "shift")) {
            MouseBinding->Flags |= Mouse_Binding_Flag_Shift;
        } else if (TokenEquals(Token, "alt")) {
            MouseBinding->Flags |= Mouse_Binding_Flag_Alt;
        } else if (TokenEquals(Token, "cmd")) {
            MouseBinding->Flags |= Mouse_Binding_Flag_Cmd;
        } else if (TokenEquals(Token, "ctrl")) {
            MouseBinding->Flags |= Mouse_Binding_Flag_Control;
        } else if (TokenIsDigit(Token)) {
            MouseBinding->Button = TokenToUnsigned(Token) - 1;
        } else if (TokenEquals(Token, "none")) {
            memset(MouseBinding, 0, sizeof(mouse_binding));
            return;
        }
    }

    MouseBinding->Active = MouseBinding->Flags != 0;
}

bool BindMouseMoveAction(const char *BindSym)
{
    ParseMouseBinding(&MouseMove, BindSym);
    return MouseMove.Active;
}

bool BindMouseResizeAction(const char *BindSym)
{
    ParseMouseBinding(&MouseResize, BindSym);
    return MouseResize.Active;
}
