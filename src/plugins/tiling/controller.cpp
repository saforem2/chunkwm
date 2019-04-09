#include "controller.h"

#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/config/cvar.h"
#include "../../common/ipc/daemon.h"
#include "../../common/misc/assert.h"

#include "presel.h"
#include "region.h"
#include "node.h"
#include "vspace.h"
#include "misc.h"
#include "constants.h"

#include <math.h>
#include <vector>
#include <poll.h>

#define internal static

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

extern macos_window_map CopyWindowCache();
extern macos_window *GetWindowByID(uint32_t Id);
extern macos_window *GetFocusedWindow();
extern uint32_t GetFocusedWindowId();
extern std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space);
extern std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space, bool IncludeInvalidWindows, bool IncludeFloatingWindows);
extern void CreateWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace);
extern void CreateDeserializedWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace);
extern void TileWindow(macos_window *Window);
extern void TileWindowOnSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace);
extern void UntileWindow(macos_window *Window);
extern void UntileWindowFromSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace);
extern bool IsWindowValid(macos_window *Window);
extern void BroadcastFocusedWindowFloating(macos_window *Window);
extern void BroadcastFocusedDesktopMode(virtual_space *VirtualSpace);

internal inline macos_space *
GetActiveSpace(macos_window *Window)
{
    macos_space *Space = NULL;;

    if (Window) {
        Space = AXLibActiveSpace(Window->Ref, Window->Id);
    } else {
        AXLibActiveSpace(&Space);
    }

    return Space;
}

internal inline macos_space *
GetActiveSpace()
{
    macos_window *Window = GetFocusedWindow();
    return GetActiveSpace(Window);
}

internal bool
IsCursorInRegion(region *Region)
{
    CGPoint Cursor = AXLibGetCursorPos();
    bool Result = ((Cursor.x >= Region->X) &&
                   (Cursor.y >= Region->Y) &&
                   (Cursor.x <= Region->X + Region->Width) &&
                   (Cursor.y <= Region->Y + Region->Height));
    return Result;
}

internal void
CenterMouseInRegion(region *Region)
{
    if (!IsCursorInRegion(Region)) {
        CGPoint Center = CGPointMake(Region->X + Region->Width / 2,
                                     Region->Y + Region->Height / 2);
        CGWarpMouseCursorPosition(Center);
    }
}

internal void
CenterMouseInWindowRef(AXUIElementRef WindowRef)
{
    region Region = CGRectToRegion(AXLibGetWindowRect(WindowRef));
    CenterMouseInRegion(&Region);
}

internal void
CenterMouseInRegionAndClick(region *Region)
{
    if (!IsCursorInRegion(Region)) {
        CGPoint Center = CGPointMake(Region->X + Region->Width / 2,
                                     Region->Y + Region->Height / 2);
        pid_t WindowPid;
        AXUIElementRef WindowRef = AXLibGetWindowAtPoint(Center, &WindowPid);

        if (!WindowRef)                   goto do_click;
        if (!AXLibGetWindowID(WindowRef)) goto free_and_do_click;

        AXLibSetFocusedWindow(WindowRef);
        AXLibSetFocusedApplication(WindowPid);

        if (StringEquals(CVarStringValue(CVAR_MOUSE_FOLLOWS_FOCUS), Mouse_Follows_Focus_Intr)) {
            CenterMouseInWindowRef(WindowRef);
        }

        CFRelease(WindowRef);
        goto out;

free_and_do_click:
        CFRelease(WindowRef);

do_click:
        CGPostMouseEvent(Center, true, 1, true);
        CGPostMouseEvent(Center, true, 1, false);
out:;
    }
}

void CenterMouseInWindow(macos_window *Window)
{
    region Region = { (float) Window->Position.x,
                      (float) Window->Position.y,
                      (float) Window->Size.width,
                      (float) Window->Size.height };
    CenterMouseInRegion(&Region);
}

enum directions
{
    Dir_Unknown,
    Dir_North,
    Dir_East,
    Dir_South,
    Dir_West,
};

internal directions
DirectionFromString(char *Direction)
{
    if      (StringEquals(Direction, "north"))  return Dir_North;
    else if (StringEquals(Direction, "east"))   return Dir_East;
    else if (StringEquals(Direction, "south"))  return Dir_South;
    else if (StringEquals(Direction, "west"))   return Dir_West;
    else                                        return Dir_Unknown;
}

internal void
WrapMonitorEdge(macos_space *Space, int Direction,
                float *X1, float *X2, float *Y1, float *Y2)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    CGRect Display = AXLibGetDisplayBounds(DisplayRef);
    CFRelease(DisplayRef);

    switch (Direction) {
    case Dir_North: { if(*Y1 < *Y2) *Y2 -= Display.size.height; } break;
    case Dir_East:  { if(*X1 > *X2) *X2 += Display.size.width;  } break;
    case Dir_South: { if(*Y1 > *Y2) *Y2 += Display.size.height; } break;
    case Dir_West:  { if(*X1 < *X2) *X2 -= Display.size.width;  } break;
    }
}

internal float
GetWindowDistance(macos_space *Space, float X1, float Y1,
                  float X2, float Y2, char *Op, bool Wrap)
{
    directions Direction = DirectionFromString(Op);
    if (Wrap) {
        WrapMonitorEdge(Space, Direction, &X1, &X2, &Y1, &Y2);
    }

    float DeltaX    = X2 - X1;
    float DeltaY    = Y2 - Y1;
    float Angle     = atan2(DeltaY, DeltaX);
    float Distance  = hypot(DeltaX, DeltaY);
    float DeltaA    = 0;

    switch (Direction) {
    case Dir_North: {
        if (DeltaY >= 0) return 0xFFFFFFFF;
        DeltaA = -M_PI_2 - Angle;
    } break;
    case Dir_East: {
        if (DeltaX <= 0) return 0xFFFFFFFF;
        DeltaA = 0.0 - Angle;
    } break;
    case Dir_South: {
        if (DeltaY <= 0) return 0xFFFFFFFF;
        DeltaA = M_PI_2 - Angle;
    } break;
    case Dir_West: {
        if (DeltaX >= 0) return 0xFFFFFFFF;
        DeltaA = M_PI - fabs(Angle);
    } break;
    case Dir_Unknown: { /* NOTE(koekeishiya) compiler warning.. */ } break;
    }

    return (Distance / cos(DeltaA / 2.0));
}

internal bool
WindowIsInDirection(char *Op, float X1, float Y1, float W1, float H1,
                    float X2, float Y2, float W2, float H2)
{
    bool Result = false;

    directions Direction = DirectionFromString(Op);
    switch (Direction) {
    case Dir_North:
    case Dir_South: {
        Result = (Y1 != Y2) && (fmax(X1, X2) < fmin(X2 + W2, X1 + W1));
    } break;
    case Dir_East:
    case Dir_West: {
        Result = (X1 != X2) && (fmax(Y1, Y2) < fmin(Y2 + H2, Y1 + H1));
    } break;
    case Dir_Unknown: { /* NOTE(koekeishiya) compiler warning.. */ } break;
    }

    return Result;
}

bool FindClosestWindow(macos_space *Space, virtual_space *VirtualSpace,
                       macos_window *Match, macos_window **ClosestWindow,
                       char *Direction, bool Wrap)
{
    float MinDist = 0xFFFFFFFF;
    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space);

    for (int Index = 0; Index < Windows.size(); ++Index) {
        macos_window *Window = GetWindowByID(Windows[Index]);
        if ((!Window) || (Match->Id == Window->Id)) continue;

        node *NodeA = GetNodeWithId(VirtualSpace->Tree, Match->Id, VirtualSpace->Mode);
        node *NodeB = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if ((!NodeA) || (!NodeB) || NodeA == NodeB) continue;

        region *A = &NodeA->Region;
        region *B = &NodeB->Region;

        if (WindowIsInDirection(Direction,
                                A->X, A->Y, A->Width, A->Height,
                                B->X, B->Y, B->Width, B->Height)) {
            float X1 = A->X + A->Width / 2;
            float Y1 = A->Y + A->Height / 2;
            float X2 = B->X + B->Width / 2;
            float Y2 = B->Y + B->Height / 2;
            float Dist = GetWindowDistance(Space, X1, Y1, X2, Y2, Direction, Wrap);
            if (Dist < MinDist) {
                MinDist = Dist;
                *ClosestWindow = Window;
            }
        }
    }

    return MinDist != 0xFFFFFFFF;
}

internal bool
FindClosestFullscreenWindow(macos_space *Space, macos_window *Match,
                            macos_window **ClosestWindow, char *Direction, bool Wrap)
{
    float MinDist = 0xFFFFFFFF;
    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space, true, false);

    char *OriginalDirection = NULL;
    if (StringEquals(Direction, "prev")) {
        OriginalDirection = Direction;
        Direction = strdup("west");
    } else if (StringEquals(Direction, "next")) {
        OriginalDirection = Direction;
        Direction = strdup("east");
    }

    for (int Index = 0; Index < Windows.size(); ++Index) {
        macos_window *Window = GetWindowByID(Windows[Index]);
        if ((!Window) || (Match->Id == Window->Id)) continue;

        macos_window *A = Match;
        macos_window *B = Window;

        if (WindowIsInDirection(Direction,
                                A->Position.x, A->Position.y, A->Size.width, A->Size.height,
                                B->Position.x, B->Position.y, B->Size.width, B->Size.height)) {
            float X1 = A->Position.x + A->Size.width / 2;
            float Y1 = A->Position.y + A->Size.height / 2;
            float X2 = B->Position.x + B->Size.width / 2;
            float Y2 = B->Position.y + B->Size.height / 2;
            float Dist = GetWindowDistance(Space, X1, Y1, X2, Y2, Direction, Wrap);
            if (Dist < MinDist) {
                MinDist = Dist;
                *ClosestWindow = Window;
            }
        }
    }

    if (OriginalDirection) {
        free(Direction);
        Direction = OriginalDirection;
    }

    return MinDist != 0xFFFFFFFF;
}

internal bool
FindWindowUndirected(macos_space *Space, virtual_space *VirtualSpace,
                     node *WindowNode, macos_window **ClosestWindow,
                     char *Direction, bool WrapMonitor)
{
    bool Result = false;
    if (StringEquals(Direction, "prev")) {
        node *PrevNode = GetPrevLeafNode(WindowNode);
        if (PrevNode) {
            *ClosestWindow = GetWindowByID(PrevNode->WindowId);
            ASSERT(*ClosestWindow);
            Result = true;
        } else if (WrapMonitor) {
            PrevNode = GetLastLeafNode(VirtualSpace->Tree);
            *ClosestWindow = GetWindowByID(PrevNode->WindowId);
            ASSERT(*ClosestWindow);
            Result = true;
        }
    } else if (StringEquals(Direction, "next")) {
        node *NextNode = GetNextLeafNode(WindowNode);
        if (NextNode) {
            *ClosestWindow = GetWindowByID(NextNode->WindowId);
            ASSERT(*ClosestWindow);
            Result = true;
        } else if (WrapMonitor) {
            NextNode = GetFirstLeafNode(VirtualSpace->Tree);
            *ClosestWindow = GetWindowByID(NextNode->WindowId);
            ASSERT(*ClosestWindow);
            Result = true;
        }
    } else if (StringEquals(Direction, "biggest")) {
        node *BiggestNode = GetBiggestLeafNode(VirtualSpace->Tree);
        if (BiggestNode) {
            *ClosestWindow = GetWindowByID(BiggestNode->WindowId);
            ASSERT(*ClosestWindow);
            Result = true;
        }
    }
    return Result;
}

void CloseWindow(char *Unused)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        AXLibCloseWindow(Window->Ref);
    }
}

internal void
FocusWindow(macos_window *Window)
{
    AXLibSetFocusedWindow(Window->Ref);
    AXLibSetFocusedApplication(Window->Owner->PSN);

    if (StringEquals(CVarStringValue(CVAR_MOUSE_FOLLOWS_FOCUS), Mouse_Follows_Focus_Intr)) {
        CenterMouseInWindow(Window);
    }
}

internal void
FocusWindowInFullscreenSpace(macos_space *Space, char *Direction)
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) return;

    char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
    bool WrapMonitor = StringEquals(FocusCycleMode, Window_Focus_Cycle_All)
                     ? AXLibDisplayCount() == 1
                     : StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor);

    macos_window *ClosestWindow;
    if (FindClosestFullscreenWindow(Space, Window, &ClosestWindow, Direction, WrapMonitor)) {
        FocusWindow(ClosestWindow);
    }
}

internal void
FocusWindowNoFocus(char *Direction)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window;
    node *Node = NULL;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        if (Space->Type == kCGSSpaceFullscreen) {
            FocusWindowInFullscreenSpace(Space, Direction);
        }
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        goto vspace_release;
    }

    if ((StringEquals(Direction, "prev")) ||
        (StringEquals(Direction, "west")) ||
        (StringEquals(Direction, "north"))) {
        Node = GetLastLeafNode(VirtualSpace->Tree);
    } else if ((StringEquals(Direction, "next")) ||
               (StringEquals(Direction, "east")) ||
               (StringEquals(Direction, "south"))) {
        Node = GetFirstLeafNode(VirtualSpace->Tree);
    }

    if (Node) {
        Window = GetWindowByID(Node->WindowId);
        ASSERT(Window);
        FocusWindow(Window);
    } else {
        char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
        ASSERT(FocusCycleMode);

        if (StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) {
            if ((StringEquals(Direction, "east")) ||
                       (StringEquals(Direction, "next"))) {
                FocusMonitor("next");
            } else if ((StringEquals(Direction, "west")) ||
                       (StringEquals(Direction, "prev"))) {
                FocusMonitor("prev");
            }
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

internal void
FocusWindowFocus(char *Direction, macos_window *Window)
{
    macos_space *Space;
    virtual_space *VirtualSpace;

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        if (Space->Type == kCGSSpaceFullscreen) {
            FocusWindowInFullscreenSpace(Space, Direction);
        }
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        goto vspace_release;
    }

    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
        ASSERT(FocusCycleMode);

        node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (WindowNode) {
            if (StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) {
                bool WrapMonitor = AXLibDisplayCount() == 1;
                macos_window *ClosestWindow;
                if ((FindWindowUndirected(Space, VirtualSpace, WindowNode, &ClosestWindow, Direction, WrapMonitor)) ||
                    (FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor))) {
                    FocusWindow(ClosestWindow);
                } else if ((StringEquals(Direction, "east")) ||
                           (StringEquals(Direction, "next"))) {
                    FocusMonitor("next");
                } else if ((StringEquals(Direction, "west")) ||
                           (StringEquals(Direction, "prev"))) {
                    FocusMonitor("prev");
                }
            } else {
                bool WrapMonitor = StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor);
                macos_window *ClosestWindow;
                if ((FindWindowUndirected(Space, VirtualSpace, WindowNode, &ClosestWindow, Direction, WrapMonitor)) ||
                    (FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor))) {
                    FocusWindow(ClosestWindow);
                }
            }
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
        ASSERT(FocusCycleMode);

        node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (WindowNode) {
            node *Node = NULL;
            if ((StringEquals(Direction, "west")) ||
                (StringEquals(Direction, "prev"))) {
                if (WindowNode->Left) {
                    Node = WindowNode->Left;
                } else if (StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) {
                    bool WrapMonitor = AXLibDisplayCount() == 1;
                    if (WrapMonitor) {
                        Node = GetLastLeafNode(VirtualSpace->Tree);
                    } else {
                        FocusMonitor("prev");
                    }
                } else if (StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor)) {
                    Node = GetLastLeafNode(VirtualSpace->Tree);
                }
            } else if ((StringEquals(Direction, "east")) ||
                       (StringEquals(Direction, "next"))) {
                if (WindowNode->Right) {
                    Node = WindowNode->Right;
                } else if (StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) {
                    bool WrapMonitor = AXLibDisplayCount() == 1;
                    if (WrapMonitor) {
                        Node = GetFirstLeafNode(VirtualSpace->Tree);
                    } else {
                        FocusMonitor("next");
                    }
                } else if (StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor)) {
                    Node = GetFirstLeafNode(VirtualSpace->Tree);
                }
            }

            if (Node) {
                macos_window *WindowToFocus = GetWindowByID(Node->WindowId);
                ASSERT(WindowToFocus);
                FocusWindow(WindowToFocus);
            }
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

void FocusWindow(char *Direction)
{
    unsigned WindowId;
    macos_window *Window;

    if (sscanf(Direction, "%d", &WindowId) == 1) {
        if ((Window = GetWindowByID(WindowId))) {
            FocusWindow(Window);
        }
    } else {
        if ((Window = GetFocusedWindow())) {
            FocusWindowFocus(Direction, Window);
        } else {
            FocusWindowNoFocus(Direction);
        }
    }
}

void SwapWindow(char *Direction)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    node *WindowNode, *ClosestNode;
    macos_window *Window, *ClosestWindow;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        goto vspace_release;
    }

    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (!WindowNode) {
            goto vspace_release;
        }

        if (!FindWindowUndirected(Space, VirtualSpace, WindowNode, &ClosestWindow, Direction, false)) {
            if (!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false)) {
                goto vspace_release;
            }
        }

        ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
        ASSERT(ClosestNode);

        SwapNodeIds(WindowNode, ClosestNode);
        ResizeWindowToRegionSize(WindowNode);
        ResizeWindowToRegionSize(ClosestNode);

        if (!StringEquals(CVarStringValue(CVAR_MOUSE_FOLLOWS_FOCUS), Mouse_Follows_Focus_Off)) {
            CenterMouseInRegion(&ClosestNode->Region);
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (!WindowNode) {
            goto vspace_release;
        }

        if ((StringEquals(Direction, "west")) || (StringEquals(Direction, "prev"))) {
            ClosestNode = WindowNode->Left
                        ? WindowNode->Left
                        : GetLastLeafNode(VirtualSpace->Tree);
        } else if((StringEquals(Direction, "east")) || (StringEquals(Direction, "next"))) {
            ClosestNode = WindowNode->Right
                        ? WindowNode->Right
                        : GetFirstLeafNode(VirtualSpace->Tree);
        } else {
            ClosestNode = NULL;
        }

        if (ClosestNode && ClosestNode != WindowNode) {
            // NOTE(koekeishiya): Swapping windows in monocle mode
            // should not trigger mouse_follows_focus.
            SwapNodeIds(WindowNode, ClosestNode);
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

void WarpWindow(char *Direction)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window, *ClosestWindow;
    node *WindowNode, *ClosestNode, *FocusedNode;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        goto vspace_release;
    }

    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        ASSERT(WindowNode);

        if (!FindWindowUndirected(Space, VirtualSpace, WindowNode, &ClosestWindow, Direction, false)) {
            if (!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false)) {
                goto vspace_release;
            }
        }

        ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
        ASSERT(ClosestNode);

        if (WindowNode->Parent == ClosestNode->Parent) {
            // NOTE(koekeishiya): Windows have the same parent, perform a regular swap.
            SwapNodeIds(WindowNode, ClosestNode);
            ResizeWindowToRegionSize(WindowNode);
            ResizeWindowToRegionSize(ClosestNode);
            FocusedNode = ClosestNode;
        } else {
            // NOTE(koekeishiya): Modify tree layout.
            UntileWindowFromSpace(Window, Space, VirtualSpace);
            UpdateCVar(CVAR_BSP_INSERTION_POINT, ClosestWindow->Id);
            TileWindowOnSpace(Window, Space, VirtualSpace);
            UpdateCVar(CVAR_BSP_INSERTION_POINT, 0);

            FocusedNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        }

        ASSERT(FocusedNode);

        if (!StringEquals(CVarStringValue(CVAR_MOUSE_FOLLOWS_FOCUS), Mouse_Follows_Focus_Off)) {
            CenterMouseInRegion(&ClosestNode->Region);
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (!WindowNode) {
            goto vspace_release;
        }

        if ((StringEquals(Direction, "west")) || (StringEquals(Direction, "prev"))) {
            ClosestNode = WindowNode->Left
                        ? WindowNode->Left
                        : GetLastLeafNode(VirtualSpace->Tree);
        } else if ((StringEquals(Direction, "east")) || (StringEquals(Direction, "next"))) {
            ClosestNode = WindowNode->Right
                        ? WindowNode->Right
                        : GetFirstLeafNode(VirtualSpace->Tree);
        } else {
            ClosestNode = NULL;
        }

        if (ClosestNode && ClosestNode != WindowNode) {
            // NOTE(koekeishiya): Swapping windows in monocle mode
            // should not trigger mouse_follows_focus.
            SwapNodeIds(WindowNode, ClosestNode);
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

void TemporaryRatio(char *Ratio)
{
    float FloatRatio;
    sscanf(Ratio, "%f", &FloatRatio);
    UpdateCVar(CVAR_BSP_SPLIT_RATIO, FloatRatio);
}

void ExtendedDockSetWindowAlpha(uint32_t WindowId, float Value, float Duration)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_alpha_fade %d %f %f", WindowId, Value, Duration);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void ExtendedDockSetWindowAlpha(uint32_t WindowId, float Value)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_alpha %d %f", WindowId, Value);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void EnableWindowFading(uint32_t FocusedWindowId)
{
    float Alpha = CVarFloatingPointValue(CVAR_WINDOW_FADE_ALPHA);
    float Duration = CVarFloatingPointValue(CVAR_WINDOW_FADE_DURATION);
    macos_window_map Copy = CopyWindowCache();

    ExtendedDockSetWindowAlpha(FocusedWindowId, 1.0f, Duration);
    for (macos_window_map_it It = Copy.begin(); It != Copy.end(); ++It) {
        macos_window *Window = It->second;
        if (Window->Id == FocusedWindowId) continue;
        ExtendedDockSetWindowAlpha(Window->Id, Alpha, Duration);
    }

    UpdateCVar(CVAR_WINDOW_FADE_INACTIVE, 1);
}

void DisableWindowFading()
{
    float Duration = CVarFloatingPointValue(CVAR_WINDOW_FADE_DURATION);
    macos_window_map Copy = CopyWindowCache();

    for (macos_window_map_it It = Copy.begin(); It != Copy.end(); ++It) {
        macos_window *Window = It->second;
        ExtendedDockSetWindowAlpha(Window->Id, 1.0f, Duration);
    }

    UpdateCVar(CVAR_WINDOW_FADE_INACTIVE, 0);
}

void ExtendedDockSetWindowPosition(uint32_t WindowId, int X, int Y)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_move %d %d %d", WindowId, X, Y);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void ExtendedDockSetWindowLevel(macos_window *Window, int WindowLevelKey)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_level %d %d", Window->Id, WindowLevelKey);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void ExtendedDockSetWindowSticky(macos_window *Window, int Value)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_sticky %d %d", Window->Id, Value);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void FloatWindow(macos_window *Window)
{
    AXLibAddFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(Window);

    if (CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST)) {
        ExtendedDockSetWindowLevel(Window, kCGModalPanelWindowLevelKey);
    }
}

internal void
UnfloatWindow(macos_window *Window)
{
    AXLibClearFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(Window);

    if (CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST)) {
        ExtendedDockSetWindowLevel(Window, kCGNormalWindowLevelKey);
    }
}

internal void
ToggleWindowFade()
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    if (CVarIntegerValue(CVAR_WINDOW_FADE_INACTIVE)) {
        DisableWindowFading();
    } else {
        EnableWindowFading(Window->Id);
    }
}

internal void
ToggleWindowFloat()
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    if (AXLibHasFlags(Window, Window_Float)) {
        UnfloatWindow(Window);
        TileWindow(Window);
    } else {
        UntileWindow(Window);
        FloatWindow(Window);
    }
}

internal void
ToggleWindowSticky()
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    if (AXLibHasFlags(Window, Window_Sticky)) {
        ExtendedDockSetWindowSticky(Window, 0);
        AXLibClearFlags(Window, Window_Sticky);

        if (AXLibHasFlags(Window, Window_Float)) {
            UnfloatWindow(Window);
            TileWindow(Window);
        }
    } else {
        ExtendedDockSetWindowSticky(Window, 1);
        AXLibAddFlags(Window, Window_Sticky);

        if (!AXLibHasFlags(Window, Window_Float)) {
            UntileWindow(Window);
            FloatWindow(Window);
        }
    }
}

internal void
ToggleWindowNativeFullscreen()
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    bool Fullscreen = AXLibIsWindowFullscreen(Window->Ref);
    if (Fullscreen) {
        AXLibSetWindowFullscreen(Window->Ref, !Fullscreen);

        if (AXLibIsWindowMovable(Window->Ref))   AXLibAddFlags(Window, Window_Movable);
        if (AXLibIsWindowResizable(Window->Ref)) AXLibAddFlags(Window, Window_Resizable);

        TileWindow(Window);
    } else {
        UntileWindow(Window);
        AXLibSetWindowFullscreen(Window->Ref, !Fullscreen);
    }
}

internal void
ToggleWindowFullscreenZoom()
{
    node *Node;
    macos_window *Window;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if (!Node) {
        goto vspace_release;
    }

    // NOTE(koekeishiya): Window is already in fullscreen-zoom, unzoom it..
    if (VirtualSpace->Tree->Zoom == Node) {
        ResizeWindowToRegionSize(Node);
        VirtualSpace->Tree->Zoom = NULL;
    } else {
        // NOTE(koekeishiya): Window is in parent-zoom, reset state.
        if (Node->Parent && Node->Parent->Zoom == Node) {
            Node->Parent->Zoom = NULL;
        }

        /*
         * NOTE(koekeishiya): Some other window is in
         * fullscreen zoom, unzoom the existing window.
         */
        if (VirtualSpace->Tree->Zoom) {
            ResizeWindowToRegionSize(VirtualSpace->Tree->Zoom);
        }

        VirtualSpace->Tree->Zoom = Node;
        ResizeWindowToExternalRegionSize(Node, VirtualSpace->Tree->Region);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

internal void
ToggleWindowParentZoom()
{
    node *Node;
    macos_window *Window;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if (!Node || !Node->Parent) {
        goto vspace_release;
    }

    // NOTE(koekeishiya): Window is already in parent-zoom, unzoom it..
    if (Node->Parent->Zoom == Node) {
        ResizeWindowToRegionSize(Node);
        Node->Parent->Zoom = NULL;
    } else {
        // NOTE(koekeishiya): Window is in fullscreen zoom, reset state.
        if (VirtualSpace->Tree->Zoom == Node) {
            VirtualSpace->Tree->Zoom = NULL;
        }

        /*
         * NOTE(koekeishiya): Some other window is in
         * parent zoom, unzoom the existing window.
         */
        if (Node->Parent->Zoom) {
            ResizeWindowToRegionSize(Node->Parent->Zoom);
        }

        Node->Parent->Zoom = Node;
        ResizeWindowToExternalRegionSize(Node, Node->Parent->Region);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

internal void
ToggleWindowSplitMode()
{
    node *Node;
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if (!Node || !Node->Parent) {
        goto vspace_release;
    }

    if (Node->Parent->Split == Split_Horizontal) {
        Node->Parent->Split = Split_Vertical;
    } else if (Node->Parent->Split == Split_Vertical) {
        Node->Parent->Split = Split_Horizontal;
    }

    CreateNodeRegionRecursive(Node->Parent, false, Space, VirtualSpace);
    ApplyNodeRegion(Node->Parent, VirtualSpace->Mode);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

void ToggleWindow(char *Type)
{
    if      (StringEquals(Type, "float"))               ToggleWindowFloat();
    else if (StringEquals(Type, "sticky"))              ToggleWindowSticky();
    else if (StringEquals(Type, "native-fullscreen"))   ToggleWindowNativeFullscreen();
    else if (StringEquals(Type, "fullscreen"))          ToggleWindowFullscreenZoom();
    else if (StringEquals(Type, "parent"))              ToggleWindowParentZoom();
    else if (StringEquals(Type, "split"))               ToggleWindowSplitMode();
    else if (StringEquals(Type, "fade"))                ToggleWindowFade();
}

void UseInsertionPoint(char *Direction)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window;
    node *Node;

    unsigned PreselectBorderColor;
    int PreselectBorderWidth;
    int PreselectBorderType;

    region PreselRegion;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if (!Node) {
        goto vspace_release;
    }

    if (VirtualSpace->Preselect) {
        if ((StringEquals(Direction, VirtualSpace->Preselect->Direction)) &&
            (VirtualSpace->Preselect->Node == Node)) {
            FreePreselectNode(VirtualSpace);
            goto vspace_release;
        } else {
            FreePreselectNode(VirtualSpace);
        }
    }

    if (StringEquals(Direction, "cancel")) {
        goto vspace_release;
    }

    VirtualSpace->Preselect = (preselect_node *) malloc(sizeof(preselect_node));
    memset(VirtualSpace->Preselect, 0, sizeof(preselect_node));

    VirtualSpace->Preselect->Direction = strdup(Direction);

    if (StringEquals(Direction, "west")) {
        VirtualSpace->Preselect->SpawnLeft = true;
        VirtualSpace->Preselect->Split = Split_Vertical;
        PreselectBorderType = PRESEL_TYPE_WEST;
    } else if (StringEquals(Direction, "east")) {
        VirtualSpace->Preselect->SpawnLeft = false;
        VirtualSpace->Preselect->Split = Split_Vertical;
        PreselectBorderType = PRESEL_TYPE_EAST;
    } else if (StringEquals(Direction, "north")) {
        VirtualSpace->Preselect->SpawnLeft = true;
        VirtualSpace->Preselect->Split = Split_Horizontal;
        PreselectBorderType = PRESEL_TYPE_NORTH;
    } else if (StringEquals(Direction, "south")) {
        VirtualSpace->Preselect->SpawnLeft = false;
        VirtualSpace->Preselect->Split = Split_Horizontal;
        PreselectBorderType = PRESEL_TYPE_SOUTH;
    } else {
        // NOTE(koekeishiya): this can't actually happen, silence compiler warning..
        PreselectBorderType = PRESEL_TYPE_NORTH;
    }

    VirtualSpace->Preselect->Node = Node;
    VirtualSpace->Preselect->Ratio = VirtualSpace->Preselect->SpawnLeft
                                   ? CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO)
                                   : 1 - CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);


    if (VirtualSpace->Preselect->Split == Split_Vertical) {
        CreatePreselectRegion(VirtualSpace->Preselect,
                              VirtualSpace->Preselect->SpawnLeft ? Region_Left : Region_Right,
                              Space,
                              VirtualSpace);
    } else if (VirtualSpace->Preselect->Split == Split_Horizontal) {
        CreatePreselectRegion(VirtualSpace->Preselect,
                              VirtualSpace->Preselect->SpawnLeft ? Region_Upper : Region_Lower,
                              Space,
                              VirtualSpace);
    }

    PreselectBorderColor = CVarUnsignedValue(CVAR_PRE_BORDER_COLOR);
    PreselectBorderWidth = CVarIntegerValue(CVAR_PRE_BORDER_WIDTH);

    PreselRegion = RoundPreselRegion(VirtualSpace->Preselect->Region, Window->Position, Window->Size);
    VirtualSpace->Preselect->Border = CreatePreselWindow(PreselectBorderType,
                                                         PreselRegion.X,
                                                         PreselRegion.Y,
                                                         PreselRegion.Width,
                                                         PreselRegion.Height,
                                                         PreselectBorderWidth,
                                                         PreselectBorderColor);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

internal void
RotateBSPTree(node *Node, char *Degrees)
{
    if ((StringEquals(Degrees, "90") && Node->Split == Split_Vertical) ||
        (StringEquals(Degrees, "270") && Node->Split == Split_Horizontal) ||
        (StringEquals(Degrees, "180"))) {
        node *Temp = Node->Left;
        Node->Left = Node->Right;
        Node->Right = Temp;
        Node->Ratio = 1 - Node->Ratio;
    }

    if (!StringEquals(Degrees, "180")) {
        if      (Node->Split == Split_Horizontal)   Node->Split = Split_Vertical;
        else if (Node->Split == Split_Vertical)     Node->Split = Split_Horizontal;
    }

    if (!IsLeafNode(Node)) {
        RotateBSPTree(Node->Left, Degrees);
        RotateBSPTree(Node->Right, Degrees);
    }
}

void RotateWindowTree(char *Degrees)
{
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    RotateBSPTree(VirtualSpace->Tree, Degrees);
    CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

internal node *
MirrorBSPTree(node *Tree, node_split Axis)
{
    if (!IsLeafNode(Tree)) {
        node *Left = MirrorBSPTree(Tree->Left, Axis);
        node *Right = MirrorBSPTree(Tree->Right, Axis);

        if (Tree->Split == Axis) {
            Tree->Left = Right;
            Tree->Right = Left;
        }
    }

    return Tree;
}

void MirrorWindowTree(char *Direction)
{
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    if (StringEquals(Direction, "vertical")) {
        VirtualSpace->Tree = MirrorBSPTree(VirtualSpace->Tree, Split_Vertical);
    } else if (StringEquals(Direction, "horizontal")) {
        VirtualSpace->Tree = MirrorBSPTree(VirtualSpace->Tree, Split_Horizontal);
    }

    CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void AdjustWindowRatio(char *Direction)
{
    macos_space *Space;
    float Offset, Ratio;
    virtual_space *VirtualSpace;
    macos_window *Window, *ClosestWindow;
    node *WindowNode, *ClosestNode, *Ancestor;

    Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    __AppleGetDisplayIdentifierFromMacOSWindow(Window);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) ||
        (VirtualSpace->Mode != Virtual_Space_Bsp) ||
        (IsLeafNode(VirtualSpace->Tree))) {
        goto vspace_release;
    }

    WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if (!WindowNode) {
        goto vspace_release;
    }

    if (!FindWindowUndirected(Space, VirtualSpace, WindowNode, &ClosestWindow, Direction, false)) {
        if (!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false)) {
            goto vspace_release;
        }
    }

    ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
    ASSERT(ClosestNode);

    Ancestor = GetLowestCommonAncestor(WindowNode, ClosestNode);
    if (!Ancestor) {
        goto vspace_release;
    }

    Offset = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
    if (!(WindowNode == Ancestor->Left || IsNodeInTree(Ancestor->Left, WindowNode))) {
        Offset = -Offset;
    }

    Ratio = Ancestor->Ratio + Offset;
    if (Ratio >= 0.1 && Ratio <= 0.9) {
        Ancestor->Ratio = Ratio;
        ResizeNodeRegion(Ancestor, Space, VirtualSpace);
        ApplyNodeRegion(Ancestor, VirtualSpace->Mode);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    __AppleFreeDisplayIdentifierFromWindow();
    AXLibDestroySpace(Space);
}

void ActivateSpaceLayout(char *Layout)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    virtual_space_mode NewLayout;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    if      (StringEquals(Layout, "bsp"))       NewLayout = Virtual_Space_Bsp;
    else if (StringEquals(Layout, "monocle"))   NewLayout = Virtual_Space_Monocle;
    else if (StringEquals(Layout, "float"))     NewLayout = Virtual_Space_Float;
    else                                        goto space_free;

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == NewLayout) {
        goto vspace_release;
    }

    if (VirtualSpace->Tree) {
        FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        VirtualSpace->Tree = NULL;
    }

    VirtualSpace->Mode = NewLayout;
    if (ShouldDeserializeVirtualSpace(VirtualSpace)) {
        CreateDeserializedWindowTreeForSpace(Space, VirtualSpace);
    } else {
        CreateWindowTreeForSpace(Space, VirtualSpace);
    }
    BroadcastFocusedDesktopMode(VirtualSpace);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void ToggleSpace(char *Op)
{
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == Virtual_Space_Float) {
        goto vspace_release;
    }

    if (StringEquals(Op, "offset")) {
        VirtualSpace->Offset = VirtualSpace->Offset
                             ? NULL
                             : &VirtualSpace->_Offset;

        if (VirtualSpace->Tree) {
            CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
            CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
            ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void AdjustSpacePadding(char *Op)
{
    macos_space *Space;
    virtual_space *VirtualSpace;
    float Delta, NewTop, NewBottom, NewLeft, NewRight;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == Virtual_Space_Float) {
        goto vspace_release;
    }

    Delta = CVarFloatingPointValue(CVAR_PADDING_STEP_SIZE);
    if (StringEquals(Op, "dec")) {
        Delta = -Delta;
    }

    NewTop = VirtualSpace->_Offset.Top + Delta;
    NewBottom = VirtualSpace->_Offset.Bottom + Delta;
    NewLeft = VirtualSpace->_Offset.Left + Delta;
    NewRight = VirtualSpace->_Offset.Right + Delta;

    if ((NewTop >= 0) && (NewBottom >= 0) &&
        (NewLeft >= 0) && (NewRight >= 0)) {
        VirtualSpace->_Offset.Top = NewTop;
        VirtualSpace->_Offset.Bottom = NewBottom;
        VirtualSpace->_Offset.Left = NewLeft;
        VirtualSpace->_Offset.Right = NewRight;
    }

    if (VirtualSpace->Tree) {
        CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
        CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
        ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void AdjustSpaceGap(char *Op)
{
    macos_space *Space;
    float Delta, NewGap;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == Virtual_Space_Float) {
        goto vspace_release;
    }

    Delta = CVarFloatingPointValue(CVAR_GAP_STEP_SIZE);
    if (StringEquals(Op, "dec")) {
        Delta = -Delta;
    }

    NewGap = VirtualSpace->_Offset.Gap + Delta;
    if (NewGap >= 0) {
        VirtualSpace->_Offset.Gap = NewGap;
    }

    if (VirtualSpace->Tree) {
        CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
        CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
        ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

// NOTE(koekeishiya): Used to properly adjust window position when moved between monitors
internal CGRect
NormalizeWindowRect(AXUIElementRef WindowRef, CFStringRef SourceMonitor, CFStringRef DestinationMonitor)
{
    CGRect Result;

    CGRect SourceBounds = AXLibGetDisplayBounds(SourceMonitor);
    CGRect DestinationBounds = AXLibGetDisplayBounds(DestinationMonitor);

    CGPoint Position = AXLibGetWindowPosition(WindowRef);
    CGSize Size = AXLibGetWindowSize(WindowRef);

    // NOTE(koekeishiya): Calculate amount of pixels between window and the monitor edge.
    float OffsetX = Position.x - SourceBounds.origin.x;
    float OffsetY = Position.y - SourceBounds.origin.y;

    // NOTE(koekeishiya): We might want to apply a scale due to different monitor resolutions.
    float ScaleX = SourceBounds.size.width / DestinationBounds.size.width;
    Result.origin.x = ScaleX > 1.0f ? OffsetX / ScaleX + DestinationBounds.origin.x
                                    : OffsetX + DestinationBounds.origin.x;

    float ScaleY = SourceBounds.size.height / DestinationBounds.size.height;
    Result.origin.y = ScaleY > 1.0f ? OffsetY / ScaleY + DestinationBounds.origin.y
                                    : OffsetY + DestinationBounds.origin.y;

    Result.size.width = Size.width / ScaleX;
    Result.size.height = Size.height / ScaleY;

    return Result;
}

bool SendWindowToDesktop(macos_window *Window, char *Op)
{
    bool Success = false, ValidWindow;
    CGRect NormalizedWindow;
    CGSSpaceID DestinationSpaceId;
    std::vector<uint32_t> WindowIds;
    unsigned SourceMonitor, DestinationMonitor;
    unsigned SourceDesktopId, DestinationDesktopId;
    macos_space *Space, *DestinationMonitorActiveSpace;
    CFStringRef SourceMonitorRef, DestinationMonitorRef;

    __AppleGetDisplayIdentifierFromMacOSWindowO(Window, SourceMonitorRef);
    ASSERT(SourceMonitorRef);

    Space = AXLibActiveSpace(SourceMonitorRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, &SourceDesktopId);
    ASSERT(Success);

    if (StringEquals(Op, "prev")) {
        DestinationDesktopId = SourceDesktopId - 1;
    } else if (StringEquals(Op, "next")) {
        DestinationDesktopId = SourceDesktopId + 1;
    } else if (sscanf(Op, "%d", &DestinationDesktopId) != 1) {
        c_log(C_LOG_LEVEL_WARN, "invalid destination desktop specified '%s'!\n", Op);
        Success = false;
        goto space_free;
    }

    if (SourceDesktopId == DestinationDesktopId) {
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination desktop specified, source desktop and destination '%d' are the same!\n",
              DestinationDesktopId);
        Success = false;
        goto space_free;
    }

    Success = AXLibCGSSpaceIDFromDesktopID(DestinationDesktopId, &DestinationMonitor, &DestinationSpaceId);
    if (!Success) {
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination desktop specified, desktop '%d' does not exist!\n",
              DestinationDesktopId);
        goto space_free;
    }

    ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
    if (ValidWindow) {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        UntileWindowFromSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibSpaceMoveWindow(DestinationSpaceId, Window->Id);

    // NOTE(koekeishiya): MacOS does not update focus when we send the window
    // to a different desktop using this method. This results in a desync causing
    // a bad user-experience.
    //
    // We retain focus on this space by giving focus to the window with the highest
    // priority as reported by MacOS. If there are no windows left on the source space,
    // we still experience desync. Not exactly sure what can be done about that.
    WindowIds = GetAllVisibleWindowsForSpace(Space, false, true);
    for (int Index = 0; Index < WindowIds.size(); ++Index) {
        if (WindowIds[Index] == Window->Id) continue;
        macos_window *Window = GetWindowByID(WindowIds[Index]);
        AXLibSetFocusedWindow(Window->Ref);
        AXLibSetFocusedApplication(Window->Owner->PSN);
        break;
    }

    if (DestinationMonitor == SourceMonitor) {
        goto space_free;
    }

    /*
     * NOTE(koekeishiya): If the destination space is on a different monitor,
     * we need to normalize the window x and y position, or it will be out of bounds.
     */
    DestinationMonitorRef = AXLibGetDisplayIdentifierFromSpace(DestinationSpaceId);
    ASSERT(DestinationMonitorRef);

    NormalizedWindow = NormalizeWindowRect(Window->Ref, SourceMonitorRef, DestinationMonitorRef);
    AXLibSetWindowPosition(Window->Ref, NormalizedWindow.origin.x, NormalizedWindow.origin.y);
    AXLibSetWindowSize(Window->Ref, NormalizedWindow.size.width, NormalizedWindow.size.height);

    // NOTE(koekeishiya): We need to update our cached window dimensions, as they are
    // used when we attempt to tile the window on the new monitor. If we don't update
    // these values, we will tile the window on the old monitor. This only happens
    // when the window is being created as the root window, using 'Region_Full'.
    Window->Position = NormalizedWindow.origin;
    Window->Size = NormalizedWindow.size;

    if (!ValidWindow) {
        goto monitor_free;
    }

    DestinationMonitorActiveSpace = AXLibActiveSpace(DestinationMonitorRef);
    if (DestinationMonitorActiveSpace->Id == DestinationSpaceId) {
        virtual_space *DestinationMonitorVirtualSpace = AcquireVirtualSpace(DestinationMonitorActiveSpace);
        TileWindowOnSpace(Window, DestinationMonitorActiveSpace, DestinationMonitorVirtualSpace);
        ReleaseVirtualSpace(DestinationMonitorVirtualSpace);
    }

    AXLibDestroySpace(DestinationMonitorActiveSpace);

monitor_free:
    CFRelease(DestinationMonitorRef);

space_free:
    __AppleFreeDisplayIdentifierFromWindowO(SourceMonitorRef);
    AXLibDestroySpace(Space);

    return Success;
}

void SendWindowToDesktop(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        SendWindowToDesktop(Window, Op);
    }
}

bool SendWindowToMonitor(macos_window *Window, char *Op)
{
    CGRect NormalizedWindow;
    bool Success, ValidWindow;
    std::vector<uint32_t> WindowIds;
    macos_space *Space, *DestinationSpace;
    unsigned SourceMonitor, DestinationMonitor;
    CFStringRef SourceMonitorRef, DestinationMonitorRef;

    __AppleGetDisplayIdentifierFromMacOSWindowO(Window, SourceMonitorRef);
    ASSERT(SourceMonitorRef);

    Space = AXLibActiveSpace(SourceMonitorRef);
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        Success = false;
        goto space_free;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, NULL);
    ASSERT(Success);

    if (StringEquals(Op, "prev")) {
        DestinationMonitor = SourceMonitor - 1;
    } else if (StringEquals(Op, "next")) {
        DestinationMonitor = SourceMonitor + 1;
    } else if (sscanf(Op, "%d", &DestinationMonitor) == 1) {
        // NOTE(koekeishiya): Convert 1-indexed back to 0-index expected by the system.
        --DestinationMonitor;
    } else {
        c_log(C_LOG_LEVEL_WARN, "invalid destination monitor specified '%s'!\n", Op);
        Success = false;
        goto space_free;
    }

    if (DestinationMonitor == SourceMonitor) {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination monitor specified, source monitor and destination '%d' are the same!\n",
              DestinationMonitor + 1);
        Success = false;
        goto space_free;
    }

    DestinationMonitorRef = AXLibGetDisplayIdentifierFromArrangement(DestinationMonitor);
    if (!DestinationMonitorRef) {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination monitor specified, monitor '%d' does not exist!\n",
              DestinationMonitor + 1);
        Success = false;
        goto space_free;
    }

    DestinationSpace = AXLibActiveSpace(DestinationMonitorRef);
    ASSERT(DestinationSpace);

    if (DestinationSpace->Type != kCGSSpaceUser) {
        goto dest_space_free;
    }

    ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
    if (ValidWindow) {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        UntileWindowFromSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibSpaceMoveWindow(DestinationSpace->Id, Window->Id);

    // NOTE(koekeishiya): MacOS does not update focus when we send the window
    // to a different monitor using this method. This results in a desync causing
    // problems with some of the MacOS APIs that we rely on.
    //
    // We retain focus on this monitor by giving focus to the window with the highest
    // priority as reported by MacOS. If there are no windows left on the source monitor,
    // we still experience desync. Not exactly sure what can be done about that.
    WindowIds = GetAllVisibleWindowsForSpace(Space, false, true);
    for (int Index = 0; Index < WindowIds.size(); ++Index) {
        if (WindowIds[Index] == Window->Id) continue;
        macos_window *Window = GetWindowByID(WindowIds[Index]);
        AXLibSetFocusedWindow(Window->Ref);
        AXLibSetFocusedApplication(Window->Owner->PSN);
        break;
    }

    /* NOTE(koekeishiya): We need to normalize the window x and y position, or it will be out of bounds. */
    NormalizedWindow = NormalizeWindowRect(Window->Ref, SourceMonitorRef, DestinationMonitorRef);
    AXLibSetWindowPosition(Window->Ref, NormalizedWindow.origin.x, NormalizedWindow.origin.y);
    AXLibSetWindowSize(Window->Ref, NormalizedWindow.size.width, NormalizedWindow.size.height);

    // NOTE(koekeishiya): We need to update our cached window dimensions, as they are
    // used when we attempt to tile the window on the new monitor. If we don't update
    // these values, we will tile the window on the old monitor. This only happens
    // when the window is being created as the root window, using 'Region_Full'.
    Window->Position = NormalizedWindow.origin;
    Window->Size = NormalizedWindow.size;

    if (ValidWindow) {
        virtual_space *DestinationVirtualSpace = AcquireVirtualSpace(DestinationSpace);
        TileWindowOnSpace(Window, DestinationSpace, DestinationVirtualSpace);
        ReleaseVirtualSpace(DestinationVirtualSpace);
    }

    __AppleFreeDisplayIdentifierFromWindowO(SourceMonitorRef);

dest_space_free:
    AXLibDestroySpace(DestinationSpace);
    CFRelease(DestinationMonitorRef);

space_free:
    AXLibDestroySpace(Space);

    return Success;
}

void SendWindowToMonitor(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        SendWindowToMonitor(Window, Op);
    }
}

internal bool
FocusMonitor(unsigned MonitorId)
{
    bool Result = false;
    std::vector<uint32_t> WindowIds;
    bool IncludeInvalidWindows;
    CFStringRef MonitorRef;
    macos_window *Window;
    macos_space *Space;

    MonitorRef = AXLibGetDisplayIdentifierFromArrangement(MonitorId);
    if (!MonitorRef) {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination monitor specified, monitor '%d' does not exist!\n",
              MonitorId + 1);
        goto out;
    }

    Space = AXLibActiveSpace(MonitorRef);
    ASSERT(Space);

    Result = true;

    IncludeInvalidWindows = (Space->Type != kCGSSpaceUser);

    WindowIds = GetAllVisibleWindowsForSpace(Space, IncludeInvalidWindows, true);
    if (WindowIds.empty()) {
        region Region = CGRectToRegion(AXLibGetDisplayBounds(MonitorRef));
        CenterMouseInRegionAndClick(&Region);
        goto space_free;
    }

    Window = GetWindowByID(WindowIds[0]);
    FocusWindow(Window);

space_free:
    AXLibDestroySpace(Space);
    CFRelease(MonitorRef);

out:
    return Result;
}

void FocusMonitor(char *Op)
{
    bool Success;
    int Operation;
    macos_space *Space;
    unsigned SourceMonitor, DestinationMonitor;

    Space = GetActiveSpace();
    ASSERT(Space);

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, NULL);
    ASSERT(Success);

    if (StringEquals(Op, "prev")) {
        DestinationMonitor = SourceMonitor - 1;
        Operation = -1;
    } else if (StringEquals(Op, "next")) {
        DestinationMonitor = SourceMonitor + 1;
        Operation = 1;
    } else if (sscanf(Op, "%d", &DestinationMonitor) == 1) {
        // NOTE(koekeishiya): Convert 1-indexed back to 0-index expected by the system.
        --DestinationMonitor;
        Operation = 0;
    } else {
        c_log(C_LOG_LEVEL_WARN, "invalid destination monitor specified '%s'!\n", Op);
        goto space_free;
    }

    if (DestinationMonitor == SourceMonitor) {
        c_log(C_LOG_LEVEL_WARN,
              "invalid destination monitor specified, source monitor and destination '%d' are the same!\n",
              DestinationMonitor + 1);
        goto space_free;
    }

    switch (Operation) {
    case -1: {
        if (!FocusMonitor(DestinationMonitor)) {
            char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
            ASSERT(FocusCycleMode);
            if ((StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) ||
                (CVarIntegerValue(CVAR_MONITOR_FOCUS_CYCLE))) {
                DestinationMonitor = AXLibDisplayCount() - 1;
                FocusMonitor(DestinationMonitor);
            }
        }
    } break;
    case 0: {
        FocusMonitor(DestinationMonitor);
    } break;
    case 1: {
        if (!FocusMonitor(DestinationMonitor)) {
            char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
            ASSERT(FocusCycleMode);
            if ((StringEquals(FocusCycleMode, Window_Focus_Cycle_All)) ||
                (CVarIntegerValue(CVAR_MONITOR_FOCUS_CYCLE))) {
                DestinationMonitor = 0;
                FocusMonitor(DestinationMonitor);
            }
        }
    } break;
    }

space_free:
    AXLibDestroySpace(Space);
}

void GridLayout(macos_window *Window, char *Op)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    if ((AXLibHasFlags(Window, Window_Float)) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        unsigned GridRows, GridCols, WinX, WinY, WinWidth, WinHeight;
        if (sscanf(Op, "%d:%d:%d:%d:%d:%d", &GridRows, &GridCols, &WinX, &WinY, &WinWidth, &WinHeight) == 6) {
            WinX = WinX >= GridCols ? GridCols - 1 : WinX;
            WinY = WinY >= GridRows ? GridRows - 1 : WinY;
            WinWidth = WinWidth <= 0 ? 1 : WinWidth;
            WinHeight = WinHeight <= 0 ? 1 : WinHeight;
            WinWidth = WinWidth > GridCols - WinX ? GridCols - WinX : WinWidth;
            WinHeight = WinHeight > GridRows - WinY ? GridRows - WinY : WinHeight;

            region Region = FullscreenRegion(DisplayRef, VirtualSpace);
            float CellWidth = Region.Width / GridCols;
            float CellHeight = Region.Height / GridRows;
            AXLibSetWindowPosition(Window->Ref,
                                   Region.X + Region.Width - CellWidth * (GridCols - WinX),
                                   Region.Y + Region.Height - CellHeight * (GridRows - WinY));
            AXLibSetWindowSize(Window->Ref, CellWidth * WinWidth, CellHeight * WinHeight);
        }
    }

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void GridLayout(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) GridLayout(Window, Op);
}

void EqualizeWindowTree(char *Unused)
{
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    EqualizeNodeTree(VirtualSpace->Tree);
    ResizeNodeRegion(VirtualSpace->Tree, Space, VirtualSpace);
    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void SerializeDesktop(char *Op)
{
    char *Buffer;
    FILE *Handle;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode != Virtual_Space_Bsp)) {
        goto vspace_release;
    }

    Buffer = SerializeNodeToBuffer(VirtualSpace->Tree);

    Handle = fopen(Op, "w");
    if (Handle) {
        size_t Length = strlen(Buffer);
        fwrite(Buffer, sizeof(char), Length, Handle);
        fclose(Handle);
    } else {
        c_log(C_LOG_LEVEL_ERROR, "failed to open '%s' for writing!\n", Op);
    }

    free(Buffer);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void DeserializeDesktop(char *Op)
{
    char *Buffer;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    ASSERT(Space);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode != Virtual_Space_Bsp) {
        goto vspace_release;
    }

    Buffer = ReadFile(Op);
    if (Buffer) {
        if (VirtualSpace->Tree) {
            FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        }

        VirtualSpace->Tree = DeserializeNodeFromBuffer(Buffer);
        CreateDeserializedWindowTreeForSpace(Space, VirtualSpace);
        free(Buffer);
    } else {
        c_log(C_LOG_LEVEL_ERROR, "failed to open '%s' for reading!\n", Op);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

internal inline CGSSpaceID
CurrentDesktopId(unsigned *DesktopId, unsigned *Arrangement, bool IncludeFullscreenSpaces)
{
    CGSSpaceID Result = 0;
    macos_space *Space = GetActiveSpace();
    if (Space) {
        AXLibCGSSpaceIDToDesktopID(Space->Id, Arrangement, DesktopId, IncludeFullscreenSpaces);
        Result = Space->Id;
        AXLibDestroySpace(Space);
    }
    return Result;
}

void FocusDesktop(char *Op)
{
    unsigned DesktopId = 0;
    unsigned Arrangement = 0;
    bool IncludeFullscreenSpaces = 0;
    if (StringEquals(Op, "prev")) {
        IncludeFullscreenSpaces = true;
        CurrentDesktopId(&DesktopId, &Arrangement, IncludeFullscreenSpaces);
        DesktopId -= 1;
    } else if (StringEquals(Op, "next")) {
        IncludeFullscreenSpaces = true;
        CurrentDesktopId(&DesktopId, &Arrangement, IncludeFullscreenSpaces);
        DesktopId += 1;
    } else if (sscanf(Op, "%d", &DesktopId) == 1) {
        CurrentDesktopId(NULL, &Arrangement, IncludeFullscreenSpaces);
    } else {
        return;
    }

    CGSSpaceID SpaceId;
    unsigned DestArrangement = 0;
    bool Success = AXLibCGSSpaceIDFromDesktopID(DesktopId, &DestArrangement, &SpaceId, IncludeFullscreenSpaces);
    if (Success) {
        int SockFD;
        if (ConnectToDaemon(&SockFD, 5050)) {
            char Message[64];
            sprintf(Message, "space %d", SpaceId);
            WriteToSocket(Message, SockFD);

            if (DestArrangement != Arrangement) {
                /*
                 *  NOTE(koekeishiya): If the target desktop is not on the same monitor
                 *  as the source desktop we switched from, the call to the Dock will not
                 *  actually make that monitor get focus. We fake a monitor focus ourselves
                 *  in these cases, after the Dock has finished processing our command.
                 */

                struct pollfd fds[] = {
                    { SockFD, POLLIN, 0 },
                    { STDOUT_FILENO, POLLHUP, 0 },
                };

                char dummy[1];
                int dummy_bytes = 0;

                while (poll(fds, 2, -1) > 0) {
                    if (fds[1].revents & (POLLERR | POLLHUP)) {
                        break;
                    }

                    if (fds[0].revents & POLLIN) {
                        if ((dummy_bytes = recv(SockFD, dummy, sizeof(dummy) - 1, 0)) <= 0) {
                            break;
                        }
                    }
                }

                FocusMonitor(DestArrangement);
            }
        }

        CloseSocket(SockFD);
    }
}

void CreateDesktop(char *Unused)
{
    int SockFD;
    macos_space *Space;

    Space = GetActiveSpace();
    if (!Space) goto out;

    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "space_create %d", Space->Id);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);

    AXLibDestroySpace(Space);
out:;
}

void DestroyDesktop(char *Unused)
{
    int SockFD;
    macos_space *Space;

    Space = GetActiveSpace();
    if (!Space) goto out;

    // NOTE(koekeishiya): Fullscreened application-spaces require special treatment
    // and can not be destroyed using this method, so we guard for improper usage.
    if (Space->Type != kCGSSpaceUser) goto space_free;

    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "space_destroy %d", Space->Id);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);

space_free:
    AXLibDestroySpace(Space);
out:;
}

void MoveDesktop(char *Op)
{
    CGSSpaceID CurrentSpaceId = 0;
    unsigned DesktopId = 0;
    unsigned CurrentArrangement = 0;
    unsigned Arrangement = 0;
    bool IncludeFullscreenSpaces = 0;
    if (StringEquals(Op, "prev")) {
        IncludeFullscreenSpaces = true;
        CurrentSpaceId = CurrentDesktopId(&DesktopId, &CurrentArrangement, IncludeFullscreenSpaces);
        Arrangement = CurrentArrangement - 1;
    } else if (StringEquals(Op, "next")) {
        IncludeFullscreenSpaces = true;
        CurrentSpaceId = CurrentDesktopId(&DesktopId, &CurrentArrangement, IncludeFullscreenSpaces);
        Arrangement = CurrentArrangement + 1;
    } else if (sscanf(Op, "%d", &Arrangement) == 1) {
        CurrentSpaceId = CurrentDesktopId(&DesktopId, &CurrentArrangement, IncludeFullscreenSpaces);
        --Arrangement;
    } else {
        return;
    }

    // TODO(koekeishiya): Do we want to allow monitor wrapping ??
    if (Arrangement == CurrentArrangement) return;
    if (Arrangement < 0) return;
    unsigned DisplayCount = AXLibDisplayCount();
    if (Arrangement > DisplayCount) return;

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromArrangement(Arrangement);
    if (!DisplayRef) return;

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);
    CFRelease(DisplayRef);

    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "space_move %d %d", CurrentSpaceId, Space->Id);
        WriteToSocket(Message, SockFD);

        struct pollfd fds[] = {
            { SockFD, POLLIN, 0 },
            { STDOUT_FILENO, POLLHUP, 0 },
        };

        char dummy[1];
        int dummy_bytes = 0;

        while (poll(fds, 2, -1) > 0) {
            if (fds[1].revents & (POLLERR | POLLHUP)) {
                break;
            }

            if (fds[0].revents & POLLIN) {
                if ((dummy_bytes = recv(SockFD, dummy, sizeof(dummy) - 1, 0)) <= 0) {
                    break;
                }
            }
        }
    }

    AXLibDestroySpace(Space);
    CloseSocket(SockFD);

    DisplayRef = AXLibGetDisplayIdentifierFromArrangement(Arrangement);
    if (DisplayRef) {
        Space = AXLibActiveSpace(DisplayRef);
        ASSERT(Space);
        CFRelease(DisplayRef);
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        VirtualSpaceAddFlags(VirtualSpace, Virtual_Space_Require_Resize);
        VirtualSpaceAddFlags(VirtualSpace, Virtual_Space_Require_Region_Update);
        ReleaseVirtualSpace(VirtualSpace);
        char Op[256] = {};
        unsigned DesktopId = 0;
        AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
        snprintf(Op, sizeof(Op), "%d", DesktopId);
        FocusDesktop(Op);
    }
}

internal void
QueryFocusedWindowFloat(int SockFD)
{
    char Message[512];
    macos_window *Window;

    Window = GetFocusedWindow();
    if (Window) {
        snprintf(Message, sizeof(Message), "%d", AXLibHasFlags(Window, Window_Float));
    } else {
        snprintf(Message, sizeof(Message), "?");
    }

    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedWindowId(int SockFD)
{
    char Message[512];
    macos_window *Window;

    Window = GetFocusedWindow();
    if (Window) {
        snprintf(Message, sizeof(Message), "%d", Window->Id);
    } else {
        snprintf(Message, sizeof(Message), "0");
    }

    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedWindowOwner(int SockFD)
{
    char Message[512];
    macos_window *Window;

    Window = GetFocusedWindow();
    if (Window) {
        snprintf(Message, sizeof(Message), "%s", Window->Owner->Name);
    } else {
        snprintf(Message, sizeof(Message), "?");
    }

    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedWindowName(int SockFD)
{
    char Message[512];
    macos_window *Window;

    Window = GetFocusedWindow();
    if (Window) {
        snprintf(Message, sizeof(Message), "%s", Window->Name);
    } else {
        snprintf(Message, sizeof(Message), "?");
    }

    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedWindowTag(int SockFD)
{
    char Message[512];
    macos_window *Window;

    Window = GetFocusedWindow();
    if (Window) {
        snprintf(Message, sizeof(Message), "%s - %s", Window->Owner->Name, Window->Name);
    } else {
        snprintf(Message, sizeof(Message), "?");
    }

    WriteToSocket(Message, SockFD);
}

internal void
QueryWindowDetails(uint32_t WindowId, int SockFD)
{
    char Buffer[1024];
    macos_window *Window = GetWindowByID(WindowId);
    if (Window) {
        char *Mainrole = Window->Mainrole ? CopyCFStringToC(Window->Mainrole) : NULL;
        char *Subrole = Window->Subrole ? CopyCFStringToC(Window->Subrole) : NULL;
        char *Name = AXLibGetWindowTitle(Window->Ref);

        snprintf(Buffer, sizeof(Buffer),
                "id: %d\n"
                "level: %d\n"
                "name: %s\n"
                "owner: %s\n"
                "role: %s\n"
                "subrole: %s\n"
                "movable: %d\n"
                "resizable: %d\n",
                Window->Id,
                Window->Level,
                Name ? Name : "<unknown>",
                Window->Owner->Name,
                Mainrole ? Mainrole : "<unknown>",
                Subrole ? Subrole : "<unknown>",
                AXLibHasFlags(Window, Window_Movable),
                AXLibHasFlags(Window, Window_Resizable));

        if (Name)     { free(Name); }
        if (Subrole)  { free(Subrole); }
        if (Mainrole) { free(Mainrole); }
    } else {
        snprintf(Buffer, sizeof(Buffer), "window not found..\n");
    }

    WriteToSocket(Buffer, SockFD);
}

void QueryWindow(char *Op, int SockFD)
{
    uint32_t WindowId;
    if (StringEquals(Op, "id")) {
        QueryFocusedWindowId(SockFD);
    } else if (StringEquals(Op, "owner")) {
        QueryFocusedWindowOwner(SockFD);
    } else if (StringEquals(Op, "name")) {
        QueryFocusedWindowName(SockFD);
    } else if (StringEquals(Op, "tag")) {
        QueryFocusedWindowTag(SockFD);
    } else if (StringEquals(Op, "float")) {
        QueryFocusedWindowFloat(SockFD);
    } else if (sscanf(Op, "%d", &WindowId) == 1) {
        QueryWindowDetails(WindowId, SockFD);
    }
}

internal void
QueryFocusedDesktop(int SockFD)
{
    char Message[512];
    macos_space *Space;
    unsigned DesktopId;
    bool Success;

    Space = GetActiveSpace();
    if (!Space) {
        snprintf(Message, sizeof(Message), "?");
        goto out;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);
    snprintf(Message, sizeof(Message), "%d", DesktopId);

    AXLibDestroySpace(Space);

out:
    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedSpaceUuid(int SockFD)
{
    char Message[512];
    macos_space *Space;
    char *IdentifierC;

    Space = GetActiveSpace();
    if (!Space) {
        snprintf(Message, sizeof(Message), "?");
        goto out;
    }

    IdentifierC = CopyCFStringToC(Space->Ref);
    snprintf(Message, sizeof(Message), "%s", IdentifierC);
    free(IdentifierC);

    AXLibDestroySpace(Space);

out:
    WriteToSocket(Message, SockFD);
}

internal void
QueryFocusedVirtualSpaceMode(int SockFD)
{
    char Message[512];
    macos_space *Space;
    virtual_space *VirtualSpace;

    Space = GetActiveSpace();
    if (!Space) {
        snprintf(Message, sizeof(Message), "?");
        goto out;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    snprintf(Message, sizeof(Message), "%s", virtual_space_mode_str[VirtualSpace->Mode]);
    ReleaseVirtualSpace(VirtualSpace);

    AXLibDestroySpace(Space);

out:
    WriteToSocket(Message, SockFD);
}

internal void
QueryWindowsForActiveSpace(int SockFD)
{
    macos_space *Space = GetActiveSpace();

    char *Cursor, *Buffer, *EndOfBuffer;
    size_t BufferSize = sizeof(char) * 4096;
    size_t BytesWritten = 0;

    Cursor = Buffer = (char *) malloc(BufferSize);
    EndOfBuffer = Buffer + BufferSize;

    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space, true, true);
    for (int Index = 0; Index < Windows.size(); ++Index) {
        ASSERT(Cursor < EndOfBuffer);

        macos_window *Window = GetWindowByID(Windows[Index]);
        ASSERT(Window);

        if (IsWindowValid(Window)) {
            BytesWritten = snprintf(Cursor, BufferSize, "%d, %s, %s\n", Window->Id, Window->Owner->Name, Window->Name);
        } else {
            BytesWritten = snprintf(Cursor, BufferSize, "%d, %s, %s (invalid)\n", Window->Id, Window->Owner->Name, Window->Name);
        }

        ASSERT(BytesWritten >= 0);
        Cursor += BytesWritten;
        BufferSize -= BytesWritten;
    }

    if (Windows.empty()) {
        snprintf(Buffer, sizeof(Buffer), "desktop is empty..\n");
    }

    WriteToSocket(Buffer, SockFD);
    free(Buffer);
    AXLibDestroySpace(Space);
}

internal void
QueryMonocleDesktopWindowCount(int SockFD)
{
    virtual_space *VirtualSpace;
    char Message[512];
    node *Node;

    unsigned int Count = 0;
    macos_space *Space = GetActiveSpace();
    if (!Space) {
        goto out;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        Node = VirtualSpace->Tree;
        while (Node) {
            ++Count;
            Node = Node->Right;
        }
    }
    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);

out:;
    snprintf(Message, sizeof(Message), "%d", Count);
    WriteToSocket(Message, SockFD);
}

internal void
QueryMonocleDesktopWindowIndex(int SockFD)
{
    virtual_space *VirtualSpace;
    macos_window *Window;
    macos_space *Space;
    char Message[512];
    node *ActiveNode;
    node *Node;
    unsigned int Index = 0;

    if (!(Window = GetFocusedWindow())) {
        goto out;
    }

    if (!(Space = GetActiveSpace(Window))) {
        goto out;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        if (VirtualSpace->Tree) {
            ActiveNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
            Node = VirtualSpace->Tree;
            while (Node) {
                ++Index;
                if (ActiveNode == Node) {
                    break;
                }
                Node = Node->Right;
            }
        }
    }
    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);

out:;
    snprintf(Message, sizeof(Message), "%d", Index);
    WriteToSocket(Message, SockFD);
}

void QueryDesktop(char *Op, int SockFD)
{
    if (StringEquals(Op, "id")) {
        QueryFocusedDesktop(SockFD);
    } else if (StringEquals(Op, "uuid")) {
        QueryFocusedSpaceUuid(SockFD);
    } else if (StringEquals(Op, "mode")) {
        QueryFocusedVirtualSpaceMode(SockFD);
    } else if (StringEquals(Op, "windows")) {
        QueryWindowsForActiveSpace(SockFD);
    } else if (StringEquals(Op, "monocle-index")) {
        QueryMonocleDesktopWindowIndex(SockFD);
    } else if (StringEquals(Op, "monocle-count")) {
        QueryMonocleDesktopWindowCount(SockFD);
    }
}

internal inline void
QueryFocusedMonitor(int SockFD)
{
    char Message[512];
    macos_space *Space;
    unsigned MonitorId;
    bool Success;

    Space = GetActiveSpace();
    if (!Space) {
        snprintf(Message, sizeof(Message), "?");
        goto out;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &MonitorId, NULL);
    ASSERT(Success);
    snprintf(Message, sizeof(Message), "%d", (MonitorId + 1));

    AXLibDestroySpace(Space);

out:
    WriteToSocket(Message, SockFD);
}

internal inline void
QueryMonitorCount(int SockFD)
{
    char Message[512];
    snprintf(Message, sizeof(Message), "%d", AXLibDisplayCount());
    WriteToSocket(Message, SockFD);
}

void QueryMonitor(char *Op, int SockFD)
{
    if (StringEquals(Op, "id")) {
        QueryFocusedMonitor(SockFD);
    } else if (StringEquals(Op, "count")) {
        QueryMonitorCount(SockFD);
    }
}

void QueryWindowsForDesktop(char *Op, int SockFD)
{
    int DesktopId;
    if (sscanf(Op, "%d", &DesktopId) != 1) return;

    CGSSpaceID SpaceId;
    unsigned Arrangement;
    bool Success = AXLibCGSSpaceIDFromDesktopID(DesktopId, &Arrangement, &SpaceId);
    ASSERT(Success);

    macos_space Space;
    Space.Id = SpaceId;

    char *Cursor, *Buffer, *EndOfBuffer;
    size_t BufferSize = sizeof(char) * 4096;
    size_t BytesWritten = 0;

    Cursor = Buffer = (char *) malloc(BufferSize);
    EndOfBuffer = Buffer + BufferSize;

    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(&Space, true, true);
    for (int Index = 0; Index < Windows.size(); ++Index) {
        ASSERT(Cursor < EndOfBuffer);

        macos_window *Window = GetWindowByID(Windows[Index]);
        ASSERT(Window);

        if (IsWindowValid(Window)) {
            BytesWritten = snprintf(Cursor, BufferSize, "%d, %s, %s\n", Window->Id, Window->Owner->Name, Window->Name);
        } else {
            BytesWritten = snprintf(Cursor, BufferSize, "%d, %s, %s (invalid)\n", Window->Id, Window->Owner->Name, Window->Name);
        }

        ASSERT(BytesWritten >= 0);
        Cursor += BytesWritten;
        BufferSize -= BytesWritten;
    }

    if (Windows.empty()) {
        *Cursor = '\0';
    }

    WriteToSocket(Buffer, SockFD);
    free(Buffer);
}

void QueryDesktopsForMonitor(char *Op, int SockFD)
{
    int MonitorId, Arrangement;
    if (sscanf(Op, "%d", &MonitorId) != 1) return;

    int DisplayCount = AXLibDisplayCount();
    if (MonitorId > DisplayCount) return;

    Arrangement = MonitorId - 1;
    if (Arrangement < 0) return;

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromArrangement(Arrangement);
    ASSERT(DisplayRef);

    int Count = 0;
    int *Desktops = AXLibSpacesForDisplay(DisplayRef, &Count);
    ASSERT(Desktops);

    size_t BufferSize = 512;
    size_t BytesWritten = 0;
    char Message[BufferSize];
    char *Cursor = Message;
    char *EndOfBuffer = Cursor + BufferSize;

    for (int Index = 0; Index < Count; ++Index) {
        ASSERT(Cursor < EndOfBuffer);
        BytesWritten = snprintf(Cursor, BufferSize, "%d ", Desktops[Index]);
        ASSERT(BytesWritten >= 0);
        Cursor += BytesWritten;
        BufferSize -= BytesWritten;
    }

    // NOTE(koekeishiya): Overwrite trailing whitespace
    Cursor[-1] = '\0';
    WriteToSocket(Message, SockFD);

    free(Desktops);
    CFRelease(DisplayRef);
}

void QueryMonitorForDesktop(char *Op, int SockFD)
{
    int DesktopId;
    if (sscanf(Op, "%d", &DesktopId) != 1) return;

    CGSSpaceID SpaceId;
    unsigned Arrangement;
    bool Success = AXLibCGSSpaceIDFromDesktopID(DesktopId, &Arrangement, &SpaceId);
    if (Success) {
        char Message[32];
        snprintf(Message, sizeof(Message), "%d", Arrangement + 1);
        WriteToSocket(Message, SockFD);
    }
}
