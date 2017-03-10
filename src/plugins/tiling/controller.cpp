#include "controller.h"

#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/ipc/daemon.h"
#include "../../common/misc/assert.h"
#include "../../common/config/cvar.h"

#include "region.h"
#include "node.h"
#include "vspace.h"
#include "misc.h"
#include "constants.h"

#include <math.h>
#include <vector>

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);
extern std::vector<uint32_t> GetAllVisibleWindows();
extern std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space);
extern void CreateWindowTree();
extern void TileWindow(macos_window *Window);
extern void TileWindowOnSpace(macos_window *Window, macos_space *Space);
extern void UntileWindow(macos_window *Window);
extern void UntileWindowFromSpace(macos_window *Window, macos_space *Space);
extern bool IsWindowValid(macos_window *Window);

internal bool
IsCursorInRegion(region Region)
{
    CGPoint Cursor = AXLibGetCursorPos();
    if(Cursor.x >= Region.X &&
       Cursor.x <= Region.X + Region.Width &&
       Cursor.y >= Region.Y &&
       Cursor.y <= Region.Y + Region.Height)
    {
        return true;
    }

    return false;
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

internal float
GetWindowDistance(macos_space *Space, virtual_space *VirtualSpace,
                  macos_window *A, macos_window *B,
                  char *Direction, bool Wrap)
{
    float X1, Y1, X2, Y2;
    GetCenterOfWindow(VirtualSpace, A, &X1, &Y1);
    GetCenterOfWindow(VirtualSpace, B, &X2, &Y2);

    bool West = StringEquals(Direction, "west");
    bool East = StringEquals(Direction, "east");
    bool North = StringEquals(Direction, "north");
    bool South = StringEquals(Direction, "south");

    if(Wrap)
    {
        CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
        CGRect Display = AXLibGetDisplayBounds(DisplayRef);
        CFRelease(DisplayRef);

        if(West && X1 < X2)
        {
            X2 -= Display.size.width;
        }
        else if(East && X1 > X2)
        {
            X2 += Display.size.width;
        }
        if(North && Y1 < Y2)
        {
            Y2 -= Display.size.height;
        }
        else if(South && Y1 > Y2)
        {
            Y2 += Display.size.height;
        }
    }

    float DeltaX = X2 - X1;
    float DeltaY = Y2 - Y1;
    float Angle = atan2(DeltaY, DeltaX);
    float Distance = hypot(DeltaX, DeltaY);
    float DeltaA = 0;

    if((North && DeltaY >= 0) ||
       (East && DeltaX <= 0) ||
       (South && DeltaY <= 0) ||
       (West && DeltaX >= 0))
    {
        return 0xFFFFFFFF;
    }

    if(North)
    {
        DeltaA = -M_PI_2 - Angle;
    }
    else if(South)
    {
        DeltaA = M_PI_2 - Angle;
    }
    else if(East)
    {
        DeltaA = 0.0 - Angle;
    }
    else if(West)
    {
        DeltaA = M_PI - fabs(Angle);
    }

    return (Distance / cos(DeltaA / 2.0));
}

internal bool
WindowIsInDirection(virtual_space *VirtualSpace,
                    macos_window *WindowA, macos_window *WindowB,
                    char *Direction)
{
    bool Result = false;

    node *NodeA = GetNodeWithId(VirtualSpace->Tree, WindowA->Id, VirtualSpace->Mode);
    node *NodeB = GetNodeWithId(VirtualSpace->Tree, WindowB->Id, VirtualSpace->Mode);

    if(NodeA && NodeB && NodeA != NodeB)
    {
        region *A = &NodeA->Region;
        region *B = &NodeB->Region;

        if((StringEquals(Direction, "north")) ||
           (StringEquals(Direction, "south")))
        {
            Result = (A->Y != B->Y) &&
                     (fmax(A->X, B->X) < fmin(B->X + B->Width, A->X + A->Width));
        }
        if((StringEquals(Direction, "east")) ||
           (StringEquals(Direction, "west")))
        {
            Result = (A->X != B->X) &&
                     (fmax(A->Y, B->Y) < fmin(B->Y + B->Height, A->Y + A->Height));
        }
    }

    return Result;
}

internal bool
FindClosestWindow(macos_space *Space, virtual_space *VirtualSpace,
                  macos_window *Match, macos_window **ClosestWindow,
                  char *Direction, bool Wrap)
{
    std::vector<uint32_t> Windows = GetAllVisibleWindows();
    float MinDist = 0xFFFFFFFF;

    for(int Index = 0;
        Index < Windows.size();
        ++Index)
    {
        macos_window *Window = GetWindowByID(Windows[Index]);
        if(Window &&
           Match->Id != Window->Id &&
           WindowIsInDirection(VirtualSpace, Match, Window, Direction))
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

void FocusWindow(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        return; // TODO(koekeishiya): Focus first or last leaf ?
    }

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            if(VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
                ASSERT(FocusCycleMode);

                if(StringEquals(FocusCycleMode, Window_Focus_Cycle_All))
                {
                    // TODO(koekeishiya): NYI
                }
                else
                {
                    bool WrapMonitor = StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor);
                    macos_window *ClosestWindow;
                    if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor))
                    {
                        AXLibSetFocusedWindow(ClosestWindow->Ref);
                        AXLibSetFocusedApplication(ClosestWindow->Owner->PSN);

                        if(CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS))
                        {
                            node *Node = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                            ASSERT(Node);

                            if(!IsCursorInRegion(Node->Region))
                            {
                                CGPoint Center = CGPointMake(Node->Region.X + Node->Region.Width / 2,
                                                             Node->Region.Y + Node->Region.Height / 2);
                                CGWarpMouseCursorPosition(Center);
                            }
                        }
                    }
                }
            }
            else if(VirtualSpace->Mode == Virtual_Space_Monocle)
            {
                node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(WindowNode)
                {
                    node *Node = NULL;
                    if(StringEquals(Direction, "west"))
                    {
                        if(WindowNode->Left)
                        {
                            Node = WindowNode->Left;
                        }
                        else
                        {
                            // TODO(koekeishiya): This code is a duplicate of the one found in SwapWindow
                            // for monocle spaces. This should be refactored when implementing different
                            // types of window_cycle_focus behaviour.
                            Node = GetLastLeafNode(VirtualSpace->Tree);
                        }
                    }
                    else if(StringEquals(Direction, "east"))
                    {
                        if(WindowNode->Right)
                        {
                            Node = WindowNode->Right;
                        }
                        else
                        {
                            // TODO(koekeishiya): This code is a duplicate of the one found in SwapWindow
                            // for monocle spaces. This should be refactored when implementing different
                            // types of window_cycle_focus behaviour.
                            Node = GetFirstLeafNode(VirtualSpace->Tree);
                        }
                    }

                    if(Node)
                    {
                        macos_window *FocusWindow = GetWindowByID(Node->WindowId);
                        ASSERT(FocusWindow);

                        AXLibSetFocusedWindow(FocusWindow->Ref);
                        AXLibSetFocusedApplication(FocusWindow->Owner->PSN);

                        if((CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS)) &&
                           (!IsCursorInRegion(Node->Region)))
                        {
                            CGPoint Center = CGPointMake(Node->Region.X + Node->Region.Width / 2,
                                                         Node->Region.Y + Node->Region.Height / 2);
                            CGWarpMouseCursorPosition(Center);
                        }
                    }
                }
            }
        }
    }

    AXLibDestroySpace(Space);
}

void SwapWindow(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(!Window)
    {
        return; // TODO(koekeishiya): Focus first or last leaf ?
    }

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            if(VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                // TODO(koekeishiya): Do we want to support this cross-monitor ??
                char *FocusCycleMode = CVarStringValue(CVAR_WINDOW_FOCUS_CYCLE);
                ASSERT(FocusCycleMode);

                node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(WindowNode)
                {
                    bool WrapMonitor = StringEquals(FocusCycleMode, Window_Focus_Cycle_Monitor);
                    macos_window *ClosestWindow;
                    if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, WrapMonitor))
                    {
                        node *ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                        ASSERT(ClosestNode);

                        SwapNodeIds(WindowNode, ClosestNode);
                        ResizeWindowToRegionSize(WindowNode);
                        ResizeWindowToRegionSize(ClosestNode);

                        if((CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS)) &&
                           (!IsCursorInRegion(ClosestNode->Region)))
                        {
                            CGPoint Center = CGPointMake(ClosestNode->Region.X + ClosestNode->Region.Width / 2,
                                                         ClosestNode->Region.Y + ClosestNode->Region.Height / 2);
                            CGWarpMouseCursorPosition(Center);
                        }
                    }
                }
            }
            else if(VirtualSpace->Mode == Virtual_Space_Monocle)
            {
                node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(WindowNode)
                {
                    node *ClosestNode = NULL;
                    if(StringEquals(Direction, "west"))
                    {
                        if(WindowNode->Left)
                        {
                            ClosestNode = WindowNode->Left;
                        }
                        else
                        {
                            // TODO(koekeishiya): This code is a duplicate of the one found in FocusWindow
                            // for monocle spaces. This should be refactored when implementing different
                            // types of window_cycle_focus behaviour.
                            ClosestNode = GetLastLeafNode(VirtualSpace->Tree);
                        }
                    }
                    else if(StringEquals(Direction, "east"))
                    {
                        if(WindowNode->Right)
                        {
                            ClosestNode = WindowNode->Right;
                        }
                        else
                        {
                            // TODO(koekeishiya): This code is a duplicate of the one found in FocusWindow
                            // for monocle spaces. This should be refactored when implementing different
                            // types of window_cycle_focus behaviour.
                            ClosestNode = GetFirstLeafNode(VirtualSpace->Tree);
                        }
                    }

                    if(ClosestNode && ClosestNode != WindowNode)
                    {
                        // NOTE(koekeishiya): Swapping windows in monocle mode
                        // should not trigger mouse_follows_focus.
                        SwapNodeIds(WindowNode, ClosestNode);
                    }
                }
            }
        }
    }

    AXLibDestroySpace(Space);
}

void WarpWindow(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(Window)
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                macos_window *ClosestWindow;
                if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, true))
                {
                    node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                    ASSERT(WindowNode);
                    node *ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                    ASSERT(ClosestNode);
                    node *FocusedNode;

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
                        UntileWindow(Window);
                        UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)ClosestWindow->Id);
                        TileWindow(Window);
                        UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)Window->Id);

                        FocusedNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                    }

                    ASSERT(FocusedNode);

                    if((CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS)) &&
                       (!IsCursorInRegion(FocusedNode->Region)))
                    {
                        CGPoint Center = CGPointMake(FocusedNode->Region.X + FocusedNode->Region.Width / 2,
                                                     FocusedNode->Region.Y + FocusedNode->Region.Height / 2);
                        CGWarpMouseCursorPosition(Center);
                    }
                }
            }
        }

        AXLibDestroySpace(Space);
    }
}

void TemporaryRatio(char *Ratio)
{
    float FloatRatio;
    sscanf(Ratio, "%f", &FloatRatio);
    UpdateCVar(CVAR_BSP_SPLIT_RATIO, FloatRatio);
}

internal void
ExtendedDockSetTopmost(macos_window *Window)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[64];
        sprintf(Message, "window_level %d %d", Window->Id, kCGFloatingWindowLevelKey);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

internal void
ExtendedDockResetTopmost(macos_window *Window)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[64];
        sprintf(Message, "window_level %d %d", Window->Id, kCGNormalWindowLevelKey);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

void FloatWindow(macos_window *Window)
{
    AXLibAddFlags(Window, Window_Float);
    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST))
    {
        ExtendedDockSetTopmost(Window);
    }
}

void UnfloatWindow(macos_window *Window)
{
    AXLibClearFlags(Window, Window_Float);
    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST))
    {
        ExtendedDockResetTopmost(Window);
    }
}

void ToggleWindow(char *Type)
{
    // NOTE(koekeishiya): We cannot use our CVAR_BSP_INSERTION_POINT here
    // because the window that we toggle options for may not be in a tree,
    // and we will not be able to perform an operation in that case.
    if(StringEquals(Type, "float"))
    {
        uint32_t WindowId = CVarIntegerValue(CVAR_FOCUSED_WINDOW);
        macos_window *Window = GetWindowByID(WindowId);
        if(Window)
        {
            if(AXLibHasFlags(Window, Window_Float))
            {
                UnfloatWindow(Window);
                TileWindow(Window);
            }
            else
            {
                UntileWindow(Window);
                FloatWindow(Window);
            }
        }
    }
    else if(StringEquals(Type, "fullscreen"))
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                uint32_t WindowId = CVarIntegerValue(CVAR_FOCUSED_WINDOW);
                node *Node = GetNodeWithId(VirtualSpace->Tree, WindowId, VirtualSpace->Mode);
                if(Node)
                {
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
                }
            }
        }

        AXLibDestroySpace(Space);
    }
    else if(StringEquals(Type, "parent"))
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                uint32_t WindowId = CVarIntegerValue(CVAR_FOCUSED_WINDOW);
                node *Node = GetNodeWithId(VirtualSpace->Tree, WindowId, VirtualSpace->Mode);
                if(Node && Node->Parent)
                {
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
                }
            }
        }

        AXLibDestroySpace(Space);
    }
    else if(StringEquals(Type, "split"))
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                uint32_t WindowId = CVarIntegerValue(CVAR_BSP_INSERTION_POINT);
                node *Node = GetNodeWithId(VirtualSpace->Tree, WindowId, VirtualSpace->Mode);
                if(Node && Node->Parent)
                {
                    if(Node->Parent->Split == Split_Horizontal)
                    {
                        Node->Parent->Split = Split_Vertical;
                    }
                    else if(Node->Parent->Split == Split_Vertical)
                    {
                        Node->Parent->Split = Split_Horizontal;
                    }

                    CreateNodeRegionRecursive(Node->Parent, false);
                    ApplyNodeRegion(Node->Parent, VirtualSpace->Mode);
                }
            }
        }

        AXLibDestroySpace(Space);
    }
}

void UseInsertionPoint(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(Window)
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                if(StringEquals(Direction, "focus"))
                {
                    UpdateCVar(CVAR_BSP_INSERTION_POINT, CVarIntegerValue(CVAR_FOCUSED_WINDOW));
                }
                else
                {
                    macos_window *ClosestWindow;
                    if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, true))
                    {
                        UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)ClosestWindow->Id);
                    }
                }
            }
        }

        AXLibDestroySpace(Space);
    }
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
        if(Node->Split == Split_Horizontal)
        {
            Node->Split = Split_Vertical;
        }
        else if(Node->Split == Split_Vertical)
        {
            Node->Split = Split_Horizontal;
        }
    }

    if(!IsLeafNode(Node))
    {
        RotateBSPTree(Node->Left, Degrees);
        RotateBSPTree(Node->Right, Degrees);
    }
}

void RotateWindowTree(char *Degrees)
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
        {
            RotateBSPTree(VirtualSpace->Tree, Degrees);
            CreateNodeRegionRecursive(VirtualSpace->Tree, false);
            ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);
        }
    }

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
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp)
        {
            if(StringEquals(Direction, "vertical"))
            {
                VirtualSpace->Tree = MirrorBSPTree(VirtualSpace->Tree, Split_Vertical);
            }
            else if(StringEquals(Direction, "horizontal"))
            {
                VirtualSpace->Tree = MirrorBSPTree(VirtualSpace->Tree, Split_Horizontal);
            }

            CreateNodeRegionRecursive(VirtualSpace->Tree, false);
            ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);
        }
    }

    AXLibDestroySpace(Space);
}

void AdjustWindowRatio(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(Window)
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            if((VirtualSpace->Tree && VirtualSpace->Mode == Virtual_Space_Bsp) &&
               (!IsLeafNode(VirtualSpace->Tree) && VirtualSpace->Tree->WindowId == 0))
            {
                node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(WindowNode)
                {
                    macos_window *ClosestWindow;
                    if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, false))
                    {
                        node *ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                        ASSERT(ClosestNode);

                        node *Ancestor = GetLowestCommonAncestor(WindowNode, ClosestNode);
                        if(Ancestor)
                        {
                            float Offset = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
                            if((IsRightChild(WindowNode)) ||
                               (IsNodeInTree(Ancestor->Right, WindowNode)))
                            {
                                Offset = -Offset;
                            }

                            float Ratio = Ancestor->Ratio + Offset;
                            if(Ratio >= 0.1 && Ratio <= 0.9)
                            {
                                Ancestor->Ratio = Ratio;
                                ResizeNodeRegion(Ancestor);
                                ApplyNodeRegion(Ancestor, VirtualSpace->Mode);
                            }
                        }
                    }
                }
            }
        }

        AXLibDestroySpace(Space);
    }
}

void ActivateSpaceLayout(char *Layout)
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space_mode NewLayout;
        if(StringEquals(Layout, "bsp"))
        {
            NewLayout = Virtual_Space_Bsp;
        }
        else if(StringEquals(Layout, "monocle"))
        {
            NewLayout = Virtual_Space_Monocle;
        }
        else if(StringEquals(Layout, "float"))
        {
            NewLayout = Virtual_Space_Float;
        }
        else
        {
            // NOTE(koekeishiya): This can never happen, silence stupid compiler warning..
            return;
        }

        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Mode != NewLayout)
        {
            if(VirtualSpace->Tree)
            {
                FreeNodeTree(VirtualSpace->Tree, VirtualSpace->Mode);
                VirtualSpace->Tree = NULL;
            }

            VirtualSpace->Mode = NewLayout;
            CreateWindowTree();
        }
    }

    AXLibDestroySpace(Space);
}

void ToggleSpace(char *Op)
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Mode != Virtual_Space_Float)
        {
            if(StringEquals(Op, "offset"))
            {
                if(VirtualSpace->Offset)
                {
                    VirtualSpace->Offset = NULL;
                }
                else
                {
                    VirtualSpace->Offset = &VirtualSpace->_Offset;
                }

                if(VirtualSpace->Tree)
                {
                    CreateNodeRegion(VirtualSpace->Tree, Region_Full);
                    CreateNodeRegionRecursive(VirtualSpace->Tree, false);
                    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
                }
            }
        }
    }

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

void SendWindowToDesktop(char *Op)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(Window)
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            unsigned SourceMonitor;
            unsigned SourceDesktopId;
            bool Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, &SourceDesktopId);
            ASSERT(Success);

            unsigned DestinationDesktopId = 0;
            if(StringEquals(Op, "prev"))
            {
                DestinationDesktopId = SourceDesktopId - 1;
            }
            else if(StringEquals(Op, "next"))
            {
                DestinationDesktopId = SourceDesktopId + 1;
            }
            else
            {
                sscanf(Op, "%d", &DestinationDesktopId);
            }

            unsigned DestinationMonitor;
            CGSSpaceID DestinationSpaceId;
            Success = AXLibCGSSpaceIDFromDesktopID(DestinationDesktopId, &DestinationMonitor, &DestinationSpaceId);
            if(Success)
            {
                bool ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
                if(ValidWindow)
                {
                    UntileWindowFromSpace(Window, Space);
                }

                AXLibSpaceAddWindow(DestinationSpaceId, Window->Id);
                AXLibSpaceRemoveWindow(Space->Id, Window->Id);

                /* NOTE(koekeishiya): If the destination space is on a different monitor,
                 * we need to normalize the window x and y position, or it will be out of bounds. */
                if(DestinationMonitor != SourceMonitor)
                {
                    CFStringRef SourceMonitorRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
                    ASSERT(SourceMonitorRef);

                    CFStringRef DestinationMonitorRef = AXLibGetDisplayIdentifierFromSpace(DestinationSpaceId);
                    ASSERT(DestinationMonitorRef);

                    CGRect Normalized = NormalizeWindowRect(Window->Ref, SourceMonitorRef, DestinationMonitorRef);
                    AXLibSetWindowPosition(Window->Ref, Normalized.origin.x, Normalized.origin.y);
                    AXLibSetWindowSize(Window->Ref, Normalized.size.width, Normalized.size.height);

                    if(ValidWindow)
                    {
                        macos_space *DestinationMonitorActiveSpace = AXLibActiveSpace(DestinationMonitorRef);
                        if((DestinationMonitorActiveSpace->Id == DestinationSpaceId) &&
                           (DestinationMonitorActiveSpace->Type == kCGSSpaceUser))
                        {
                            TileWindowOnSpace(Window, DestinationMonitorActiveSpace);
                        }

                        AXLibDestroySpace(DestinationMonitorActiveSpace);
                    }

                    CFRelease(DestinationMonitorRef);
                    CFRelease(SourceMonitorRef);
                }

                // NOTE(koekeishiya): MacOS does not update focus when we send the window
                // to a different desktop using this method. This results in a desync causing
                // a bad user-experience.
                //
                // We retain focus on this space by giving focus to the window with the highest
                // priority as reported by MacOS. If there are no windows left on the source space,
                // we still experience desync. Not exactly sure what can be done about that.
                std::vector<uint32_t> WindowIds = GetAllVisibleWindowsForSpace(Space);
                if(!WindowIds.empty())
                {
                    uint32_t WindowId = WindowIds[0];
                    macos_window *Window = GetWindowByID(WindowId);
                    AXLibSetFocusedWindow(Window->Ref);
                    AXLibSetFocusedApplication(Window->Owner->PSN);
                }
            }
            else
            {
                fprintf(stderr,
                        "invalid destination desktop specified, desktop '%d' does not exist!\n",
                        DestinationDesktopId);
            }
        }

        AXLibDestroySpace(Space);
    }
}

void SendWindowToMonitor(char *Op)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(CVAR_BSP_INSERTION_POINT));
    if(Window)
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(Space->Type == kCGSSpaceUser)
        {
            unsigned SourceMonitor;
            bool Success = AXLibCGSSpaceIDToDesktopID(Space->Id, &SourceMonitor, NULL);
            ASSERT(Success);

            unsigned DestinationMonitor;
            if(StringEquals(Op, "prev"))
            {
                DestinationMonitor = SourceMonitor - 1;
            }
            else if(StringEquals(Op, "next"))
            {
                DestinationMonitor = SourceMonitor + 1;
            }
            else
            {
                sscanf(Op, "%d", &DestinationMonitor);
            }

            if(DestinationMonitor != SourceMonitor)
            {
                CFStringRef DestinationMonitorRef = AXLibGetDisplayIdentifierFromArrangement(DestinationMonitor);
                if(DestinationMonitorRef)
                {
                    macos_space *DestinationSpace = AXLibActiveSpace(DestinationMonitorRef);
                    if(DestinationSpace->Type == kCGSSpaceUser)
                    {
                        bool ValidWindow = ((!AXLibHasFlags(Window, Window_Float)) && (IsWindowValid(Window)));
                        if(ValidWindow)
                        {
                            UntileWindowFromSpace(Window, Space);
                        }

                        AXLibSpaceAddWindow(DestinationSpace->Id, Window->Id);
                        AXLibSpaceRemoveWindow(Space->Id, Window->Id);

                        CFStringRef SourceMonitorRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
                        ASSERT(SourceMonitorRef);

                        /* NOTE(koekeishiya): We need to normalize the window x and y position, or it will be out of bounds. */
                        CGRect Normalized = NormalizeWindowRect(Window->Ref, SourceMonitorRef, DestinationMonitorRef);
                        AXLibSetWindowPosition(Window->Ref, Normalized.origin.x, Normalized.origin.y);
                        AXLibSetWindowSize(Window->Ref, Normalized.size.width, Normalized.size.height);

                        if(ValidWindow)
                        {
                            TileWindowOnSpace(Window, DestinationSpace);
                        }

                        CFRelease(DestinationMonitorRef);
                        CFRelease(SourceMonitorRef);

                        // NOTE(koekeishiya): MacOS does not update focus when we send the window
                        // to a different monitor using this method. This results in a desync causing
                        // problems with some of the MacOS APIs that we rely on.
                        //
                        // We retain focus on this monitor by giving focus to the window with the highest
                        // priority as reported by MacOS. If there are no windows left on the source monitor,
                        // we still experience desync. Not exactly sure what can be done about that.
                        std::vector<uint32_t> WindowIds = GetAllVisibleWindowsForSpace(Space);
                        if(!WindowIds.empty())
                        {
                            uint32_t WindowId = WindowIds[0];
                            macos_window *Window = GetWindowByID(WindowId);
                            AXLibSetFocusedWindow(Window->Ref);
                            AXLibSetFocusedApplication(Window->Owner->PSN);
                        }
                    }

                    AXLibDestroySpace(DestinationSpace);
                }
                else
                {
                    fprintf(stderr,
                            "invalid destination monitor specified, monitor '%d' does not exist!\n",
                            DestinationMonitor);
                }
            }
        }

        AXLibDestroySpace(Space);
    }
}
