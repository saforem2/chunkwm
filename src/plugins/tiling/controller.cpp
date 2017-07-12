#include "controller.h"

#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/border/border.h"
#include "../../common/config/cvar.h"
#include "../../common/ipc/daemon.h"
#include "../../common/misc/assert.h"
#include "../../common/misc/debug.h"

#include "region.h"
#include "node.h"
#include "vspace.h"
#include "misc.h"
#include "constants.h"

#include <math.h>
#include <vector>

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);
extern macos_window *GetFocusedWindow();
extern std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space);
extern std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space, bool IncludeInvalidWindows, bool IncludeFloatingWindows);
extern void CreateWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace);
extern void CreateDeserializedWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace);
extern void TileWindow(macos_window *Window);
extern void TileWindowOnSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace);
extern void UntileWindow(macos_window *Window);
extern void UntileWindowFromSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace);
extern bool IsWindowValid(macos_window *Window);
extern void BroadcastFocusedWindowFloating(int Status);

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
    if(!IsCursorInRegion(Region))
    {
        CGPoint Center = CGPointMake(Region->X + Region->Width / 2,
                                     Region->Y + Region->Height / 2);
        CGWarpMouseCursorPosition(Center);
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

internal void
GetCenterOfWindow(virtual_space *VirtualSpace, macos_window *Window, float *X, float *Y)
{
    node *Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if(Node)
    {
        *X = Node->Region.X + Node->Region.Width / 2;
        *Y = Node->Region.Y + Node->Region.Height / 2;
    }
    else
    {
        *X = -1;
        *Y = -1;
    }
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
    if     (StringEquals(Direction, "north"))   return Dir_North;
    else if(StringEquals(Direction, "east"))    return Dir_East;
    else if(StringEquals(Direction, "south"))   return Dir_South;
    else if(StringEquals(Direction, "west"))    return Dir_West;
    else                                        return Dir_Unknown;
}

internal void
WrapMonitorEdge(macos_space *Space, int Direction,
                float *X1, float *X2, float *Y1, float *Y2)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    CGRect Display = AXLibGetDisplayBounds(DisplayRef);
    CFRelease(DisplayRef);

    switch(Direction)
    {
        case Dir_North: { if(*Y1 < *Y2) *Y2 -= Display.size.height; } break;
        case Dir_East:  { if(*X1 > *X2) *X2 += Display.size.width;  } break;
        case Dir_South: { if(*Y1 > *Y2) *Y2 += Display.size.height; } break;
        case Dir_West:  { if(*X1 < *X2) *X2 -= Display.size.width;  } break;
    }
}

internal float
GetWindowDistance(macos_space *Space, virtual_space *VirtualSpace,
                  macos_window *A, macos_window *B,
                  char *Op, bool Wrap)
{
    float X1, Y1, X2, Y2;
    GetCenterOfWindow(VirtualSpace, A, &X1, &Y1);
    GetCenterOfWindow(VirtualSpace, B, &X2, &Y2);

    directions Direction = DirectionFromString(Op);
    if(Wrap)
    {
        WrapMonitorEdge(Space, Direction, &X1, &X2, &Y1, &Y2);
    }

    float DeltaX    = X2 - X1;
    float DeltaY    = Y2 - Y1;
    float Angle     = atan2(DeltaY, DeltaX);
    float Distance  = hypot(DeltaX, DeltaY);
    float DeltaA    = 0;

    switch(Direction)
    {
        case Dir_North:
        {
            if(DeltaY >= 0) return 0xFFFFFFFF;
            DeltaA = -M_PI_2 - Angle;
        } break;
        case Dir_East:
        {
            if(DeltaX <= 0) return 0xFFFFFFFF;
            DeltaA = 0.0 - Angle;
        } break;
        case Dir_South:
        {
            if(DeltaY <= 0) return 0xFFFFFFFF;
            DeltaA = M_PI_2 - Angle;
        } break;
        case Dir_West:
        {
            if(DeltaX >= 0) return 0xFFFFFFFF;
            DeltaA = M_PI - fabs(Angle);
        } break;
        case Dir_Unknown: { /* NOTE(koekeishiya) compiler warning.. */ } break;
    }

    return (Distance / cos(DeltaA / 2.0));
}

internal bool
WindowIsInDirection(virtual_space *VirtualSpace, char *Op,
                    macos_window *WindowA, macos_window *WindowB)
{
    bool Result = false;

    node *NodeA = GetNodeWithId(VirtualSpace->Tree, WindowA->Id, VirtualSpace->Mode);
    node *NodeB = GetNodeWithId(VirtualSpace->Tree, WindowB->Id, VirtualSpace->Mode);

    if(NodeA && NodeB && NodeA != NodeB)
    {
        region *A = &NodeA->Region;
        region *B = &NodeB->Region;

        directions Direction = DirectionFromString(Op);
        switch(Direction)
        {
            case Dir_North:
            case Dir_South:
            {
                Result = (A->Y != B->Y) &&
                         (fmax(A->X, B->X) < fmin(B->X + B->Width, A->X + A->Width));
            } break;
            case Dir_East:
            case Dir_West:
            {
                Result = (A->X != B->X) &&
                         (fmax(A->Y, B->Y) < fmin(B->Y + B->Height, A->Y + A->Height));
            } break;
            case Dir_Unknown: { /* NOTE(koekeishiya) compiler warning.. */ } break;
        }
    }

    return Result;
}

bool FindClosestWindow(macos_space *Space, virtual_space *VirtualSpace,
                       macos_window *Match, macos_window **ClosestWindow,
                       char *Direction, bool Wrap)
{
    float MinDist = 0xFFFFFFFF;
    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space);

    for(int Index = 0; Index < Windows.size(); ++Index)
    {
        macos_window *Window = GetWindowByID(Windows[Index]);
        if((Window) &&
           (Match->Id != Window->Id) &&
           (WindowIsInDirection(VirtualSpace, Direction, Match, Window)))
        {
            float Dist = GetWindowDistance(Space, VirtualSpace,
                                           Match, Window,
                                           Direction, Wrap);
            if(Dist < MinDist)
            {
                MinDist = Dist;
                *ClosestWindow = Window;
            }
        }
    }

    return MinDist != 0xFFFFFFFF;
}

internal bool
FindWindowUndirected(macos_space *Space, virtual_space *VirtualSpace,
                     macos_window *Window, macos_window **ClosestWindow,
                     char *Direction, bool WrapMonitor)
{
    bool Result = false;
    if(StringEquals(Direction, "prev"))
    {
        node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(WindowNode)
        {
            node *PrevNode = GetPrevLeafNode(WindowNode);
            if(PrevNode)
            {
                *ClosestWindow = GetWindowByID(PrevNode->WindowId);
                ASSERT(*ClosestWindow);
                Result = true;
            }
            else if(WrapMonitor)
            {
                PrevNode = GetLastLeafNode(VirtualSpace->Tree);
                *ClosestWindow = GetWindowByID(PrevNode->WindowId);
                ASSERT(*ClosestWindow);
                Result = true;
            }
        }
    }
    else if(StringEquals(Direction, "next"))
    {
        node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(WindowNode)
        {
            node *NextNode = GetNextLeafNode(WindowNode);
            if(NextNode)
            {
                *ClosestWindow = GetWindowByID(NextNode->WindowId);
                ASSERT(*ClosestWindow);
                Result = true;
            }
            else if(WrapMonitor)
            {
                NextNode = GetFirstLeafNode(VirtualSpace->Tree);
                *ClosestWindow = GetWindowByID(NextNode->WindowId);
                ASSERT(*ClosestWindow);
                Result = true;
            }
        }
    }
    return Result;
}

void FocusWindow(char *Direction)
{
    bool Success;
    macos_space *Space;
    macos_window *Window;
    virtual_space *VirtualSpace;

    Window = GetWindowByID(CVarUnsignedValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        return; // TODO(koekeishiya): Focus first or last leaf ?
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode == Virtual_Space_Float))
    {
        goto vspace_release;
    }

    if(VirtualSpace->Mode == Virtual_Space_Bsp)
    {
        char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
        ASSERT(FocusCycleMode);

        if(StringEquals(FocusCycleMode, Window_Focus_Cycle_All))
        {
            macos_window *ClosestWindow;
            if((FindWindowUndirected(Space, VirtualSpace, Window, &ClosestWindow, Direction, false)) ||
               (FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false)))
            {
                AXLibSetFocusedWindow(ClosestWindow->Ref);
                AXLibSetFocusedApplication(ClosestWindow->Owner->PSN);
            }
            else if((StringEquals(Direction, "east")) ||
                    (StringEquals(Direction, "next")))
            {
                FocusMonitor("next");
            }
            else if((StringEquals(Direction, "west")) ||
                    (StringEquals(Direction, "prev")))
            {
                FocusMonitor("prev");
            }
        }
        else
        {
            bool WrapMonitor = StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor);
            macos_window *ClosestWindow;
            if((FindWindowUndirected(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor)) ||
               (FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor)))
            {
                AXLibSetFocusedWindow(ClosestWindow->Ref);
                AXLibSetFocusedApplication(ClosestWindow->Owner->PSN);
            }
        }
    }
    else if(VirtualSpace->Mode == Virtual_Space_Monocle)
    {
        char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
        ASSERT(FocusCycleMode);

        node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(WindowNode)
        {
            node *Node = NULL;
            if((StringEquals(Direction, "west")) ||
               (StringEquals(Direction, "prev")))
            {
                if     (WindowNode->Left)                                           Node = WindowNode->Left;
                else if(StringEquals(FocusCycleMode, Window_Focus_Cycle_All))       FocusMonitor("prev");
                else if(StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor))   Node = GetLastLeafNode(VirtualSpace->Tree);
            }
            else if((StringEquals(Direction, "east")) ||
                    (StringEquals(Direction, "next")))
            {
                if     (WindowNode->Right)                                          Node = WindowNode->Right;
                else if(StringEquals(FocusCycleMode, Window_Focus_Cycle_All))       FocusMonitor("next");
                else if(StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor))   Node = GetFirstLeafNode(VirtualSpace->Tree);
            }

            if(Node)
            {
                macos_window *FocusWindow = GetWindowByID(Node->WindowId);
                ASSERT(FocusWindow);

                AXLibSetFocusedWindow(FocusWindow->Ref);
                AXLibSetFocusedApplication(FocusWindow->Owner->PSN);
            }
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void SwapWindow(char *Direction)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;
    node *WindowNode, *ClosestNode;
    macos_window *Window, *ClosestWindow;

    Window = GetWindowByID(CVarUnsignedValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        goto out;
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode == Virtual_Space_Float))
    {
        goto vspace_release;
    }

    if(VirtualSpace->Mode == Virtual_Space_Bsp)
    {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(!WindowNode)
        {
            goto vspace_release;
        }

        if(!FindWindowUndirected(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
        {
            if(!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
            {
                goto vspace_release;
            }
        }

        ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
        ASSERT(ClosestNode);

        SwapNodeIds(WindowNode, ClosestNode);
        ResizeWindowToRegionSize(WindowNode);
        ResizeWindowToRegionSize(ClosestNode);

        if(CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS))
        {
            CenterMouseInRegion(&ClosestNode->Region);
        }
    }
    else if(VirtualSpace->Mode == Virtual_Space_Monocle)
    {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(!WindowNode)
        {
            goto vspace_release;
        }

        if((StringEquals(Direction, "west")) ||
           (StringEquals(Direction, "prev")))
        {
            ClosestNode = WindowNode->Left
                        ? WindowNode->Left
                        : GetLastLeafNode(VirtualSpace->Tree);
        }
        else if((StringEquals(Direction, "east")) ||
                (StringEquals(Direction, "next")))
        {
            ClosestNode = WindowNode->Right
                        ? WindowNode->Right
                        : GetFirstLeafNode(VirtualSpace->Tree);
        }
        else
        {
            ClosestNode = NULL;
        }

        if(ClosestNode && ClosestNode != WindowNode)
        {
            // NOTE(koekeishiya): Swapping windows in monocle mode
            // should not trigger mouse_follows_focus.
            SwapNodeIds(WindowNode, ClosestNode);
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
out:;
}

void WarpWindow(char *Direction)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window, *ClosestWindow;
    node *WindowNode, *ClosestNode, *FocusedNode;

    Window = GetWindowByID(CVarUnsignedValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        goto out;
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode == Virtual_Space_Float))
    {
        goto vspace_release;
    }

    if(VirtualSpace->Mode == Virtual_Space_Bsp)
    {
        if(!FindWindowUndirected(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
        {
            if(!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
            {
                goto vspace_release;
            }
        }

        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        ASSERT(WindowNode);
        ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
        ASSERT(ClosestNode);

        if(WindowNode->Parent == ClosestNode->Parent)
        {
            // NOTE(koekeishiya): Windows have the same parent, perform a regular swap.
            SwapNodeIds(WindowNode, ClosestNode);
            ResizeWindowToRegionSize(WindowNode);
            ResizeWindowToRegionSize(ClosestNode);
            FocusedNode = ClosestNode;
        }
        else
        {
            // NOTE(koekeishiya): Modify tree layout.
            UntileWindowFromSpace(Window, Space, VirtualSpace);
            UpdateCVar(CVAR_BSP_INSERTION_POINT, ClosestWindow->Id);
            TileWindowOnSpace(Window, Space, VirtualSpace);
            UpdateCVar(CVAR_BSP_INSERTION_POINT, Window->Id);

            FocusedNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        }

        ASSERT(FocusedNode);

        if(CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS))
        {
            CenterMouseInRegion(&FocusedNode->Region);
        }
    }
    else if(VirtualSpace->Mode == Virtual_Space_Monocle)
    {
        WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(!WindowNode)
        {
            goto vspace_release;
        }

        if((StringEquals(Direction, "west")) ||
           (StringEquals(Direction, "prev")))
        {
            ClosestNode = WindowNode->Left
                        ? WindowNode->Left
                        : GetLastLeafNode(VirtualSpace->Tree);
        }
        else if((StringEquals(Direction, "east")) ||
                (StringEquals(Direction, "next")))
        {
            ClosestNode = WindowNode->Right
                        ? WindowNode->Right
                        : GetFirstLeafNode(VirtualSpace->Tree);
        }
        else
        {
            ClosestNode = NULL;
        }

        if(ClosestNode && ClosestNode != WindowNode)
        {
            // NOTE(koekeishiya): Swapping windows in monocle mode
            // should not trigger mouse_follows_focus.
            SwapNodeIds(WindowNode, ClosestNode);
        }
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
out:;
}

void TemporaryRatio(char *Ratio)
{
    float FloatRatio;
    sscanf(Ratio, "%f", &FloatRatio);
    UpdateCVar(CVAR_BSP_SPLIT_RATIO, FloatRatio);
}

internal void
ExtendedDockSetWindowLevel(macos_window *Window, int WindowLevelKey)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[64];
        sprintf(Message, "window_level %d %d", Window->Id, WindowLevelKey);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

internal void
ExtendedDockSetWindowSticky(macos_window *Window, int Value)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[64];
        sprintf(Message, "window_sticky %d %d", Window->Id, Value);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

internal void
CenterWindow(macos_window *Window)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    CGRect DisplayFrame = AXLibGetDisplayBounds(DisplayRef);

    AXLibSetWindowPosition(Window->Ref,
                           DisplayFrame.origin.x + DisplayFrame.size.width / 4,
                           DisplayFrame.origin.y + DisplayFrame.size.height / 4);

    AXLibSetWindowSize(Window->Ref,
                       DisplayFrame.size.width / 2,
                       DisplayFrame.size.height / 2);

    CFRelease(DisplayRef);
}

void FloatWindow(macos_window *Window, bool UserInitiated)
{
    AXLibAddFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(1);

    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST))
    {
        ExtendedDockSetWindowLevel(Window, kCGFloatingWindowLevelKey);
    }

    if((UserInitiated) &&
       (CVarIntegerValue(CVAR_WINDOW_FLOAT_CENTER)))
    {
        CenterWindow(Window);
    }
}

internal void
UnfloatWindow(macos_window *Window)
{
    AXLibClearFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(0);

    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST))
    {
        ExtendedDockSetWindowLevel(Window, kCGNormalWindowLevelKey);
    }
}

internal void
ToggleWindowFloat()
{
    macos_window *Window = GetFocusedWindow();
    if(!Window)
    {
        return;
    }

    if(AXLibHasFlags(Window, Window_Float))
    {
        UnfloatWindow(Window);
        TileWindow(Window);
    }
    else
    {
        UntileWindow(Window);
        FloatWindow(Window, true);
    }
}

internal void
ToggleWindowSticky()
{
    macos_window *Window = GetFocusedWindow();
    if(!Window)
    {
        return;
    }

    if(AXLibHasFlags(Window, Window_Sticky))
    {
        ExtendedDockSetWindowSticky(Window, 0);
        AXLibClearFlags(Window, Window_Sticky);

        if(AXLibHasFlags(Window, Window_Float))
        {
            UnfloatWindow(Window);
            TileWindow(Window);
        }
    }
    else
    {
        ExtendedDockSetWindowSticky(Window, 1);
        AXLibAddFlags(Window, Window_Sticky);

        if(!AXLibHasFlags(Window, Window_Float))
        {
            UntileWindow(Window);
            FloatWindow(Window, true);
        }
    }
}

internal void
ToggleWindowNativeFullscreen()
{
    macos_window *Window = GetFocusedWindow();
    if(!Window)
    {
        return;
    }

    bool Fullscreen = AXLibIsWindowFullscreen(Window->Ref);
    if(Fullscreen)
    {
        AXLibSetWindowFullscreen(Window->Ref, !Fullscreen);

        if(AXLibIsWindowMovable(Window->Ref))   AXLibAddFlags(Window, Window_Movable);
        if(AXLibIsWindowResizable(Window->Ref)) AXLibAddFlags(Window, Window_Resizable);

        TileWindow(Window);
    }
    else
    {
        UntileWindow(Window);
        AXLibSetWindowFullscreen(Window->Ref, !Fullscreen);
    }
}

internal void
ToggleWindowFullscreenZoom()
{
    node *Node;
    bool Success;
    macos_window *Window;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    Window = GetFocusedWindow();
    if(!Window)
    {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if(!Node)
    {
        goto vspace_release;
    }

    // NOTE(koekeishiya): Window is already in fullscreen-zoom, unzoom it..
    if(VirtualSpace->Tree->Zoom == Node)
    {
        ResizeWindowToRegionSize(Node);
        VirtualSpace->Tree->Zoom = NULL;
    }
    else
    {
        // NOTE(koekeishiya): Window is in parent-zoom, reset state.
        if(Node->Parent && Node->Parent->Zoom == Node)
        {
            Node->Parent->Zoom = NULL;
        }

        /* NOTE(koekeishiya): Some other window is in
         * fullscreen zoom, unzoom the existing window. */
        if(VirtualSpace->Tree->Zoom)
        {
            ResizeWindowToRegionSize(VirtualSpace->Tree->Zoom);
        }

        VirtualSpace->Tree->Zoom = Node;
        ResizeWindowToExternalRegionSize(Node, VirtualSpace->Tree->Region);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

internal void
ToggleWindowParentZoom()
{
    node *Node;
    bool Success;
    macos_window *Window;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    Window = GetFocusedWindow();
    if(!Window)
    {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if(!Node || !Node->Parent)
    {
        goto vspace_release;
    }

    // NOTE(koekeishiya): Window is already in parent-zoom, unzoom it..
    if(Node->Parent->Zoom == Node)
    {
        ResizeWindowToRegionSize(Node);
        Node->Parent->Zoom = NULL;
    }
    else
    {
        // NOTE(koekeishiya): Window is in fullscreen zoom, reset state.
        if(VirtualSpace->Tree->Zoom == Node)
        {
            VirtualSpace->Tree->Zoom = NULL;
        }

        /* NOTE(koekeishiya): Some other window is in
         * parent zoom, unzoom the existing window. */
        if(Node->Parent->Zoom)
        {
            ResizeWindowToRegionSize(Node->Parent->Zoom);
        }

        Node->Parent->Zoom = Node;
        ResizeWindowToExternalRegionSize(Node, Node->Parent->Region);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

internal void
ToggleWindowSplitMode()
{
    node *Node;
    bool Success;
    uint32_t WindowId;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    WindowId = CVarUnsignedValue(CVAR_BSP_INSERTION_POINT);
    Node = GetNodeWithId(VirtualSpace->Tree, WindowId, VirtualSpace->Mode);
    if(!Node || !Node->Parent)
    {
        goto vspace_release;
    }

    if(Node->Parent->Split == Split_Horizontal)
    {
        Node->Parent->Split = Split_Vertical;
    }
    else if(Node->Parent->Split == Split_Vertical)
    {
        Node->Parent->Split = Split_Horizontal;
    }

    CreateNodeRegionRecursive(Node->Parent, false, Space, VirtualSpace);
    ApplyNodeRegion(Node->Parent, VirtualSpace->Mode);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void ToggleWindow(char *Type)
{
    // NOTE(koekeishiya): We cannot use our CVAR_BSP_INSERTION_POINT here
    // because the window that we toggle options for may not be in a tree,
    // and we will not be able to perform an operation in that case.
    if     (StringEquals(Type, "float"))               ToggleWindowFloat();
    else if(StringEquals(Type, "sticky"))              ToggleWindowSticky();
    else if(StringEquals(Type, "native-fullscreen"))   ToggleWindowNativeFullscreen();
    else if(StringEquals(Type, "fullscreen"))          ToggleWindowFullscreenZoom();
    else if(StringEquals(Type, "parent"))              ToggleWindowParentZoom();
    else if(StringEquals(Type, "split"))               ToggleWindowSplitMode();
}

void UseInsertionPoint(char *Direction)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;
    macos_window *Window;
    node *Node;

    unsigned PreselectBorderColor;
    int PreselectBorderWidth;
    int PreselectBorderRadius;

    Window = GetFocusedWindow();
    if(!Window)
    {
        goto out;
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if(!Node)
    {
        goto vspace_release;
    }

    if(Node->Preselect)
    {
        if(StringEquals(Direction, Node->Preselect->Direction))
        {
            FreePreselectNode(Node);
            goto vspace_release;
        }
        else
        {
            FreePreselectNode(Node);
        }
    }

    if(StringEquals(Direction, "cancel"))
    {
        goto vspace_release;
    }

    Node->Preselect = (preselect_node *) malloc(sizeof(preselect_node));
    memset(Node->Preselect, 0, sizeof(preselect_node));

    Node->Preselect->Direction = strdup(Direction);

    if(StringEquals(Direction, "west"))
    {
        Node->Preselect->SpawnLeft = true;
        Node->Preselect->Split = Split_Vertical;
    }
    else if(StringEquals(Direction, "east"))
    {
        Node->Preselect->SpawnLeft = false;
        Node->Preselect->Split = Split_Vertical;
    }
    else if(StringEquals(Direction, "north"))
    {
        Node->Preselect->SpawnLeft = true;
        Node->Preselect->Split = Split_Horizontal;
    }
    else if(StringEquals(Direction, "south"))
    {
        Node->Preselect->SpawnLeft = false;
        Node->Preselect->Split = Split_Horizontal;
    }

    Node->Preselect->Node = Node;
    Node->Preselect->Ratio = Node->Preselect->SpawnLeft
                           ? CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO)
                           : 1 - CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);


    if(Node->Preselect->Split == Split_Vertical)
    {
        CreatePreselectRegion(Node->Preselect,
                              Node->Preselect->SpawnLeft ? Region_Left : Region_Right,
                              Space,
                              VirtualSpace);
    }
    else if(Node->Preselect->Split == Split_Horizontal)
    {
        CreatePreselectRegion(Node->Preselect,
                              Node->Preselect->SpawnLeft ? Region_Upper : Region_Lower,
                              Space,
                              VirtualSpace);
    }

    PreselectBorderColor = CVarUnsignedValue(CVAR_PRE_BORDER_COLOR);
    PreselectBorderWidth = CVarIntegerValue(CVAR_PRE_BORDER_WIDTH);
    PreselectBorderRadius = CVarIntegerValue(CVAR_PRE_BORDER_RADIUS);

    Node->Preselect->Border = CreateBorderWindow(Node->Preselect->Region.X,
                                                 Node->Preselect->Region.Y,
                                                 Node->Preselect->Region.Width,
                                                 Node->Preselect->Region.Height,
                                                 PreselectBorderWidth,
                                                 PreselectBorderRadius,
                                                 PreselectBorderColor,
                                                 false);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
out:;
}

internal void
RotateBSPTree(node *Node, char *Degrees)
{
    if((StringEquals(Degrees, "90") && Node->Split == Split_Vertical) ||
       (StringEquals(Degrees, "270") && Node->Split == Split_Horizontal) ||
       (StringEquals(Degrees, "180")))
    {
        node *Temp = Node->Left;
        Node->Left = Node->Right;
        Node->Right = Temp;
        Node->Ratio = 1 - Node->Ratio;
    }

    if(!StringEquals(Degrees, "180"))
    {
        if     (Node->Split == Split_Horizontal)   Node->Split = Split_Vertical;
        else if(Node->Split == Split_Vertical)     Node->Split = Split_Horizontal;
    }

    if(!IsLeafNode(Node))
    {
        RotateBSPTree(Node->Left, Degrees);
        RotateBSPTree(Node->Right, Degrees);
    }
}

void RotateWindowTree(char *Degrees)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
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
    if(!IsLeafNode(Tree))
    {
        node *Left = MirrorBSPTree(Tree->Left, Axis);
        node *Right = MirrorBSPTree(Tree->Right, Axis);

        if(Tree->Split == Axis)
        {
            Tree->Left = Right;
            Tree->Right = Left;
        }
    }

    return Tree;
}

void MirrorWindowTree(char *Direction)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    if(StringEquals(Direction, "vertical"))
    {
        VirtualSpace->Tree = MirrorBSPTree(VirtualSpace->Tree, Split_Vertical);
    }
    else if(StringEquals(Direction, "horizontal"))
    {
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
    bool Success;
    macos_space *Space;
    float Offset, Ratio;
    virtual_space *VirtualSpace;
    macos_window *Window, *ClosestWindow;
    node *WindowNode, *ClosestNode, *Ancestor;

    Window = GetWindowByID(CVarUnsignedValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        goto out;
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp) ||
       (IsLeafNode(VirtualSpace->Tree)))
    {
        goto vspace_release;
    }

    WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
    if(!WindowNode)
    {
        goto vspace_release;
    }

    if(!FindWindowUndirected(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
    {
        if(!FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
        {
            goto vspace_release;
        }
    }

    ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
    ASSERT(ClosestNode);

    Ancestor = GetLowestCommonAncestor(WindowNode, ClosestNode);
    if(!Ancestor)
    {
        goto vspace_release;
    }

    Offset = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
    if((IsRightChild(WindowNode)) ||
       (IsNodeInTree(Ancestor->Right, WindowNode)))
    {
        Offset = -Offset;
    }

    Ratio = Ancestor->Ratio + Offset;
    if(Ratio >= 0.1 && Ratio <= 0.9)
    {
        Ancestor->Ratio = Ratio;
        ResizeNodeRegion(Ancestor, Space, VirtualSpace);
        ApplyNodeRegion(Ancestor, VirtualSpace->Mode);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
out:;
}

void ActivateSpaceLayout(char *Layout)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;
    virtual_space_mode NewLayout;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    if     (StringEquals(Layout, "bsp"))       NewLayout = Virtual_Space_Bsp;
    else if(StringEquals(Layout, "monocle"))   NewLayout = Virtual_Space_Monocle;
    else if(StringEquals(Layout, "float"))     NewLayout = Virtual_Space_Float;
    else                                       goto space_free;

    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode == NewLayout)
    {
        goto vspace_release;
    }

    if(VirtualSpace->Tree)
    {
        FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        VirtualSpace->Tree = NULL;
    }

    VirtualSpace->Mode = NewLayout;
    if(ShouldDeserializeVirtualSpace(VirtualSpace))
    {
        CreateDeserializedWindowTreeForSpace(Space, VirtualSpace);
    }
    else
    {
        CreateWindowTreeForSpace(Space, VirtualSpace);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void ToggleSpace(char *Op)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode == Virtual_Space_Float)
    {
        goto vspace_release;
    }

    if(StringEquals(Op, "offset"))
    {
        VirtualSpace->Offset = VirtualSpace->Offset
                             ? NULL
                             : &VirtualSpace->_Offset;

        if(VirtualSpace->Tree)
        {
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
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;
    float Delta, NewTop, NewBottom, NewLeft, NewRight;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode == Virtual_Space_Float)
    {
        goto vspace_release;
    }

    Delta = CVarFloatingPointValue(CVAR_PADDING_STEP_SIZE);
    if(StringEquals(Op, "dec"))
    {
        Delta = -Delta;
    }

    NewTop = VirtualSpace->_Offset.Top + Delta;
    NewBottom = VirtualSpace->_Offset.Bottom + Delta;
    NewLeft = VirtualSpace->_Offset.Left + Delta;
    NewRight = VirtualSpace->_Offset.Right + Delta;

    if((NewTop >= 0) &&
       (NewBottom >= 0) &&
       (NewLeft >= 0) &&
       (NewRight >= 0))
    {
        VirtualSpace->_Offset.Top = NewTop;
        VirtualSpace->_Offset.Bottom = NewBottom;
        VirtualSpace->_Offset.Left = NewLeft;
        VirtualSpace->_Offset.Right = NewRight;
    }

    if(VirtualSpace->Tree)
    {
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
    bool Success;
    macos_space *Space;
    float Delta, NewGap;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode == Virtual_Space_Float)
    {
        goto vspace_release;
    }

    Delta = CVarFloatingPointValue(CVAR_GAP_STEP_SIZE);
    if(StringEquals(Op, "dec"))
    {
        Delta = -Delta;
    }

    NewGap = VirtualSpace->_Offset.Gap + Delta;
    if(NewGap >= 0)
    {
        VirtualSpace->_Offset.Gap = NewGap;
    }

    if(VirtualSpace->Tree)
    {
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
    macos_space **SpacesForWindow, **List;
    unsigned SourceMonitor, DestinationMonitor;
    unsigned SourceDesktopId, DestinationDesktopId;
    macos_space *Space, *DestinationMonitorActiveSpace;
    CFStringRef SourceMonitorRef, DestinationMonitorRef;

    SpacesForWindow = List = AXLibSpacesForWindow(Window->Id);
    Space = *List++;
    ASSERT(Space);
    ASSERT(!(*List));
    free(SpacesForWindow);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, &SourceDesktopId);
    ASSERT(Success);

    if(StringEquals(Op, "prev"))
    {
        DestinationDesktopId = SourceDesktopId - 1;
    }
    else if(StringEquals(Op, "next"))
    {
        DestinationDesktopId = SourceDesktopId + 1;
    }
    else if(sscanf(Op, "%d", &DestinationDesktopId) != 1)
    {
        fprintf(stderr, "invalid destination desktop specified '%s'!\n", Op);
        Success = false;
        goto space_free;
    }

    if(SourceDesktopId == DestinationDesktopId)
    {
        fprintf(stderr,
                "invalid destination desktop specified, source desktop and destination '%d' are the same!\n",
                DestinationDesktopId);
        Success = false;
        goto space_free;
    }

    Success = AXLibCGSSpaceIDFromDesktopID(DestinationDesktopId, &DestinationMonitor, &DestinationSpaceId);
    if(!Success)
    {
        fprintf(stderr,
                "invalid destination desktop specified, desktop '%d' does not exist!\n",
                DestinationDesktopId);
        goto space_free;
    }

    ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
    if(ValidWindow)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        UntileWindowFromSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibSpaceAddWindow(DestinationSpaceId, Window->Id);
    AXLibSpaceRemoveWindow(Space->Id, Window->Id);

    // NOTE(koekeishiya): MacOS does not update focus when we send the window
    // to a different desktop using this method. This results in a desync causing
    // a bad user-experience.
    //
    // We retain focus on this space by giving focus to the window with the highest
    // priority as reported by MacOS. If there are no windows left on the source space,
    // we still experience desync. Not exactly sure what can be done about that.
    WindowIds = GetAllVisibleWindowsForSpace(Space);
    for(int Index = 0;
        Index < WindowIds.size();
        ++Index)
    {
        if(WindowIds[Index] == Window->Id) continue;
        macos_window *Window = GetWindowByID(WindowIds[Index]);
        AXLibSetFocusedWindow(Window->Ref);
        AXLibSetFocusedApplication(Window->Owner->PSN);
        break;
    }

    if(DestinationMonitor == SourceMonitor)
    {
        goto space_free;
    }

    /* NOTE(koekeishiya): If the destination space is on a different monitor,
     * we need to normalize the window x and y position, or it will be out of bounds. */
    SourceMonitorRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(SourceMonitorRef);

    DestinationMonitorRef = AXLibGetDisplayIdentifierFromSpace(DestinationSpaceId);
    ASSERT(DestinationMonitorRef);

    NormalizedWindow = NormalizeWindowRect(Window->Ref, SourceMonitorRef, DestinationMonitorRef);
    AXLibSetWindowPosition(Window->Ref, NormalizedWindow.origin.x, NormalizedWindow.origin.y);
    AXLibSetWindowSize(Window->Ref, NormalizedWindow.size.width, NormalizedWindow.size.height);

    if(!ValidWindow)
    {
        goto monitor_free;
    }

    DestinationMonitorActiveSpace = AXLibActiveSpace(DestinationMonitorRef);
    if(DestinationMonitorActiveSpace->Id == DestinationSpaceId)
    {
        virtual_space *DestinationMonitorVirtualSpace = AcquireVirtualSpace(DestinationMonitorActiveSpace);
        TileWindowOnSpace(Window, DestinationMonitorActiveSpace, DestinationMonitorVirtualSpace);
        ReleaseVirtualSpace(DestinationMonitorVirtualSpace);
    }

    AXLibDestroySpace(DestinationMonitorActiveSpace);

monitor_free:
    CFRelease(DestinationMonitorRef);
    CFRelease(SourceMonitorRef);

space_free:
    AXLibDestroySpace(Space);

    return Success;
}

void SendWindowToDesktop(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if(Window)
    {
        SendWindowToDesktop(Window, Op);
    }
}

void SendWindowToMonitor(char *Op)
{
    macos_window *Window;
    CGRect NormalizedWindow;
    bool Success, ValidWindow;
    std::vector<uint32_t> WindowIds;
    macos_space *Space, *DestinationSpace;
    unsigned SourceMonitor, DestinationMonitor;
    CFStringRef SourceMonitorRef, DestinationMonitorRef;

    Window = GetFocusedWindow();
    if(!Window)
    {
        goto out;
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, NULL);
    ASSERT(Success);

    if(StringEquals(Op, "prev"))
    {
        DestinationMonitor = SourceMonitor - 1;
    }
    else if(StringEquals(Op, "next"))
    {
        DestinationMonitor = SourceMonitor + 1;
    }
    else if(sscanf(Op, "%d", &DestinationMonitor) == 1)
    {
        // NOTE(koekeishiya): Convert 1-indexed back to 0-index expected by the system.
        --DestinationMonitor;
    }
    else
    {
        fprintf(stderr, "invalid destination monitor specified '%s'!\n", Op);
        goto space_free;
    }

    if(DestinationMonitor == SourceMonitor)
    {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        fprintf(stderr,
                "invalid destination monitor specified, source monitor and destination '%d' are the same!\n",
                DestinationMonitor + 1);
        goto space_free;
    }

    DestinationMonitorRef = AXLibGetDisplayIdentifierFromArrangement(DestinationMonitor);
    if(!DestinationMonitorRef)
    {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        fprintf(stderr,
                "invalid destination monitor specified, monitor '%d' does not exist!\n",
                DestinationMonitor + 1);
        goto space_free;
    }

    DestinationSpace = AXLibActiveSpace(DestinationMonitorRef);
    ASSERT(DestinationSpace);

    if(DestinationSpace->Type != kCGSSpaceUser)
    {
        goto dest_space_free;
    }

    ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
    if(ValidWindow)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        UntileWindowFromSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibSpaceAddWindow(DestinationSpace->Id, Window->Id);
    AXLibSpaceRemoveWindow(Space->Id, Window->Id);

    // NOTE(koekeishiya): MacOS does not update focus when we send the window
    // to a different monitor using this method. This results in a desync causing
    // problems with some of the MacOS APIs that we rely on.
    //
    // We retain focus on this monitor by giving focus to the window with the highest
    // priority as reported by MacOS. If there are no windows left on the source monitor,
    // we still experience desync. Not exactly sure what can be done about that.
    WindowIds = GetAllVisibleWindowsForSpace(Space);
    for(int Index = 0;
        Index < WindowIds.size();
        ++Index)
    {
        if(WindowIds[Index] == Window->Id) continue;
        macos_window *Window = GetWindowByID(WindowIds[Index]);
        AXLibSetFocusedWindow(Window->Ref);
        AXLibSetFocusedApplication(Window->Owner->PSN);
        break;
    }

    SourceMonitorRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(SourceMonitorRef);

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

    if(ValidWindow)
    {
        virtual_space *DestinationVirtualSpace = AcquireVirtualSpace(DestinationSpace);
        TileWindowOnSpace(Window, DestinationSpace, DestinationVirtualSpace);
        ReleaseVirtualSpace(DestinationVirtualSpace);
    }

    CFRelease(SourceMonitorRef);

dest_space_free:
    AXLibDestroySpace(DestinationSpace);
    CFRelease(DestinationMonitorRef);

space_free:
    AXLibDestroySpace(Space);
out:;
}

void FocusMonitor(char *Op)
{
    bool Success;
    macos_window *Window;
    std::vector<uint32_t> WindowIds;
    CFStringRef DestinationMonitorRef;
    macos_space *Space, *DestinationSpace;
    unsigned SourceMonitor, DestinationMonitor;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, NULL);
    ASSERT(Success);

    if(StringEquals(Op, "prev"))
    {
        DestinationMonitor = SourceMonitor - 1;
    }
    else if(StringEquals(Op, "next"))
    {
        DestinationMonitor = SourceMonitor + 1;
    }
    else if(sscanf(Op, "%d", &DestinationMonitor) == 1)
    {
        // NOTE(koekeishiya): Convert 1-indexed back to 0-index expected by the system.
        --DestinationMonitor;
    }
    else
    {
        fprintf(stderr, "invalid destination monitor specified '%s'!\n", Op);
        goto space_free;
    }

    if(DestinationMonitor == SourceMonitor)
    {
        fprintf(stderr,
                "invalid destination monitor specified, source monitor and destination '%d' are the same!\n",
                DestinationMonitor + 1);
        goto space_free;
    }

    DestinationMonitorRef = AXLibGetDisplayIdentifierFromArrangement(DestinationMonitor);
    if(!DestinationMonitorRef)
    {
        // NOTE(koekeishiya): Convert 0-indexed back to 1-index when printng error to user.
        fprintf(stderr,
                "invalid destination monitor specified, monitor '%d' does not exist!\n",
                DestinationMonitor + 1);
        goto space_free;
    }

    DestinationSpace = AXLibActiveSpace(DestinationMonitorRef);
    ASSERT(DestinationSpace);
    if(DestinationSpace->Type != kCGSSpaceUser)
    {
        goto dest_space_free;
    }

    WindowIds = GetAllVisibleWindowsForSpace(DestinationSpace);
    if(WindowIds.empty())
    {
        goto dest_space_free;
    }

    Window = GetWindowByID(WindowIds[0]);
    AXLibSetFocusedWindow(Window->Ref);
    AXLibSetFocusedApplication(Window->Owner->PSN);

dest_space_free:
    AXLibDestroySpace(DestinationSpace);
    CFRelease(DestinationMonitorRef);

space_free:
    AXLibDestroySpace(Space);
}

void WarpFloatingWindow(char *Op)
{
    region Region;
    macos_space *Space;
    macos_window *Window;
    CFStringRef DisplayRef;
    virtual_space *VirtualSpace;

    Window = GetFocusedWindow();
    if(!Window)
    {
        goto out;
    }

    DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    VirtualSpace = AcquireVirtualSpace(Space);

    if((!AXLibHasFlags(Window, Window_Float)) &&
       (VirtualSpace->Mode != Virtual_Space_Float))
    {
        goto space_free;
    }

    Region = CGRectToRegion(AXLibGetDisplayBounds(DisplayRef));
    ConstrainRegion(DisplayRef, &Region);

    if(StringEquals(Op, "fullscreen"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);
    }
    else if(StringEquals(Op, "left"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height);
    }
    else if(StringEquals(Op, "right"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X + Region.Width / 2, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height);
    }
    else if(StringEquals(Op, "top-left"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height / 2);
    }
    else if(StringEquals(Op, "top-right"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X + Region.Width / 2, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height / 2);
    }
    else if(StringEquals(Op, "bottom-left"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y + Region.Height / 2);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height / 2);
    }
    else if(StringEquals(Op, "bottom-right"))
    {
        AXLibSetWindowPosition(Window->Ref, Region.X + Region.Width / 2, Region.Y + Region.Height / 2);
        AXLibSetWindowSize(Window->Ref, Region.Width / 2, Region.Height / 2);
    }

space_free:
    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
out:;
}

void EqualizeWindowTree(char *Unused)
{
    bool Success;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
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
    bool Success;
    char *Buffer;
    FILE *Handle;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if((!VirtualSpace->Tree) ||
       (VirtualSpace->Mode != Virtual_Space_Bsp))
    {
        goto vspace_release;
    }

    Buffer = SerializeNodeToBuffer(VirtualSpace->Tree);

    Handle = fopen(Op, "w");
    if(Handle)
    {
        size_t Length = strlen(Buffer);
        fwrite(Buffer, sizeof(char), Length, Handle);
        fclose(Handle);
    }
    else
    {
        fprintf(stderr, "failed to open '%s' for writing!\n", Op);
    }

    free(Buffer);

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

void DeserializeDesktop(char *Op)
{
    bool Success;
    char *Buffer;
    macos_space *Space;
    virtual_space *VirtualSpace;

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        goto space_free;
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    if(VirtualSpace->Mode != Virtual_Space_Bsp)
    {
        goto vspace_release;
    }

    Buffer = ReadFile(Op);
    if(Buffer)
    {
        if(VirtualSpace->Tree)
        {
            FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
        }

        VirtualSpace->Tree = DeserializeNodeFromBuffer(Buffer);
        CreateDeserializedWindowTreeForSpace(Space, VirtualSpace);
        free(Buffer);
    }
    else
    {
        fprintf(stderr, "failed to open '%s' for reading!\n", Op);
    }

vspace_release:
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);
}

// NOTE(koekeishiya): Caller is responsible for memory.
char *QueryWindowDetails(uint32_t WindowId)
{
    char *Result = NULL;

    macos_window *Window = GetWindowByID(WindowId);
    if(Window)
    {
        char Details[1024];
        char *Mainrole = Window->Mainrole ? CopyCFStringToC(Window->Mainrole) : NULL;
        char *Subrole = Window->Subrole ? CopyCFStringToC(Window->Subrole) : NULL;
        char *Name = AXLibGetWindowTitle(Window->Ref);

        snprintf(Details, sizeof(Details),
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

        if(Name) { free(Name); }
        if(Subrole) { free(Subrole); }
        if(Mainrole) { free(Mainrole); }

        Result = strdup(Details);
    }

    return Result;
}

// NOTE(koekeishiya): Caller is responsible for memory.
char *QueryWindowsForActiveSpace()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    char *Result = NULL;
    char Buffer[1024] = {};
    char Temp[64] = {};

    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space, true, true);
    for(int Index = 0; Index < Windows.size(); ++Index)
    {
        macos_window *Window = GetWindowByID(Windows[Index]);
        ASSERT(Window);

        if(IsWindowValid(Window))
        {
            snprintf(Temp, sizeof(Temp), "%d, %s\n", Window->Id, Window->Owner->Name);
        }
        else
        {
            snprintf(Temp, sizeof(Temp), "%d, %s (invalid)\n", Window->Id, Window->Owner->Name);
        }
        strcat(Buffer, Temp);
    }

    if(!Windows.empty())
    {
        Result = strdup(Buffer);
    }

    return Result;
}
