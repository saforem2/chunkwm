#include "controller.h"

#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
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

    // TODO(koekeishiya): Pass info so that we can figure out which
    // display we are currently on. Should this be stored in VirtualSpace ??
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
    macos_window *Window = GetWindowByID(CVarIntegerValue(_CVAR_BSP_INSERTION_POINT));
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
                macos_window *ClosestWindow;
                if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, true))
                {
                    AXLibSetFocusedWindow(ClosestWindow->Ref);
                    AXLibSetFocusedApplication(ClosestWindow->Owner->PSN);

                    if(CVarIntegerValue(CVAR_MOUSE_FOLLOWS_FOCUS))
                    {
                        node *Node = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                        if(Node && !IsCursorInRegion(Node->Region))
                        {
                            CGPoint Center = CGPointMake(Node->Region.X + Node->Region.Width / 2,
                                                         Node->Region.Y + Node->Region.Height / 2);
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
    macos_window *Window = GetWindowByID(CVarIntegerValue(_CVAR_BSP_INSERTION_POINT));
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
                node *WindowNode = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(WindowNode)
                {
                    macos_window *ClosestWindow;
                    if(FindClosestWindow(Space, VirtualSpace, Window, &ClosestWindow, Direction, true))
                    {
                        node *ClosestNode = GetNodeWithId(VirtualSpace->Tree, ClosestWindow->Id, VirtualSpace->Mode);
                        if(ClosestNode)
                        {
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
            }
            else if(VirtualSpace->Mode == Virtual_Space_Monocle)
            {
                // TODO(koekeishiya): NYI
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

void UseInsertionPoint(char *Direction)
{
    macos_window *Window = GetWindowByID(CVarIntegerValue(_CVAR_BSP_INSERTION_POINT));
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
                    UpdateCVar(_CVAR_BSP_INSERTION_POINT, (int)ClosestWindow->Id);
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

    if(Node->Left)
    {
        RotateBSPTree(Node->Left, Degrees);
    }

    if(Node->Right)
    {
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
