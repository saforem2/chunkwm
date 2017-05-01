#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"

#include <queue>
#include <map>

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

node_split OptimalSplitMode(node *Node)
{
    float OptimalRatio = CVarFloatingPointValue(CVAR_BSP_OPTIMAL_RATIO);

    float NodeRatio = Node->Region.Width / Node->Region.Height;
    return NodeRatio >= OptimalRatio ? Split_Vertical : Split_Horizontal;
}

node *CreateRootNode(uint32_t WindowId)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->WindowId = WindowId;
    CreateNodeRegion(Node, Region_Full);
    Node->Split = OptimalSplitMode(Node);
    Node->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    return Node;
}

node *CreateLeafNode(node *Parent, uint32_t WindowId, region_type Type)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->Parent = Parent;
    Node->WindowId = WindowId;
    CreateNodeRegion(Node, Type);
    Node->Split = OptimalSplitMode(Node);
    Node->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    return Node;
}

void CreateLeafNodePair(node *Parent, uint32_t ExistingWindowId, uint32_t SpawnedWindowId, node_split Split)
{
    Parent->WindowId = 0;
    Parent->Split = Split;
    Parent->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    uint32_t LeftWindowId, RightWindowId;
    if(CVarIntegerValue(CVAR_BSP_SPAWN_LEFT))
    {
        LeftWindowId = SpawnedWindowId;
        RightWindowId = ExistingWindowId;
    }
    else
    {
        LeftWindowId = ExistingWindowId;
        RightWindowId = SpawnedWindowId;
    }

    ASSERT(Split == Split_Vertical || Split == Split_Horizontal);
    if(Split == Split_Vertical)
    {
        Parent->Left = CreateLeafNode(Parent, LeftWindowId, Region_Left);
        Parent->Right = CreateLeafNode(Parent, RightWindowId, Region_Right);
    }
    else if(Split == Split_Horizontal)
    {
        Parent->Left = CreateLeafNode(Parent, LeftWindowId, Region_Upper);
        Parent->Right = CreateLeafNode(Parent, RightWindowId, Region_Lower);
    }
}

internal inline void
CenterWindowInRegion(macos_window *Window, region Region)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    float DiffX = (Region.X + Region.Width) - (Position.x + Size.width);
    float DiffY = (Region.Y + Region.Height) - (Position.y + Size.height);

    if((DiffX > 0.0f) ||
       (DiffY > 0.0f))
    {
        float OffsetX = DiffX / 2.0f;
        Region.X += OffsetX;
        Region.Width -= OffsetX;

        float OffsetY = DiffY / 2.0f;
        Region.Y += OffsetY;
        Region.Height -= OffsetY;

        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);
    }
}

void ResizeWindowToRegionSize(node *Node, bool Center)
{
    // NOTE(koekeishiya): GetWindowByID should not be able to fail!
    macos_window *Window = GetWindowByID(Node->WindowId);
    ASSERT(Window);

    bool WindowMoved  = AXLibSetWindowPosition(Window->Ref, Node->Region.X, Node->Region.Y);
    bool WindowResized = AXLibSetWindowSize(Window->Ref, Node->Region.Width, Node->Region.Height);

    if(Center)
    {
        if(WindowMoved || WindowResized)
        {
            CenterWindowInRegion(Window, Node->Region);
        }
    }
}

// NOTE(koekeishiya): Call ResizeWindowToRegionSize with center -> true
void ResizeWindowToRegionSize(node *Node)
{
    ResizeWindowToRegionSize(Node, true);
}

void ResizeWindowToExternalRegionSize(node *Node, region Region, bool Center)
{
    // NOTE(koekeishiya): GetWindowByID should not be able to fail!
    macos_window *Window = GetWindowByID(Node->WindowId);
    ASSERT(Window);

    bool WindowMoved  = AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
    bool WindowResized = AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);

    if(Center)
    {
        if(WindowMoved || WindowResized)
        {
            CenterWindowInRegion(Window, Region);
        }
    }
}

// NOTE(koekeishiya): Call ResizeWindowToExternalRegionSize with center -> true
void ResizeWindowToExternalRegionSize(node *Node, region Region)
{
    ResizeWindowToExternalRegionSize(Node, Region, true);
}

void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode, bool Center)
{
    if(Node->WindowId)
    {
        ResizeWindowToRegionSize(Node, Center);
    }

    if(Node->Left && VirtualSpaceMode == Virtual_Space_Bsp)
    {
        ApplyNodeRegion(Node->Left, VirtualSpaceMode, Center);
    }

    if(Node->Right)
    {
        ApplyNodeRegion(Node->Right, VirtualSpaceMode, Center);
    }
}

// NOTE(koekeishiya): Call ApplyNodeRegion with center -> true
void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode)
{
    ApplyNodeRegion(Node, VirtualSpaceMode, true);
}

void FreeNodeTree(node *Node, virtual_space_mode VirtualSpaceMode)
{
    if(Node->Left && VirtualSpaceMode == Virtual_Space_Bsp)
    {
        FreeNodeTree(Node->Left, VirtualSpaceMode);
    }

    if(Node->Right)
    {
        FreeNodeTree(Node->Right, VirtualSpaceMode);
    }

    free(Node);
}

bool IsRightChild(node *Node)
{
    bool Result = Node->Parent && Node->Parent->Right == Node;
    return Result;
}

bool IsLeftChild(node *Node)
{
    bool Result = Node->Parent && Node->Parent->Left == Node;
    return Result;
}

bool IsNodeInTree(node *Tree, node *Node)
{
    bool Result = ((Tree->Left == Node || Tree->Right == Node) ||
                   (Tree->Left && IsNodeInTree(Tree->Left, Node)) ||
                   (Tree->Right && IsNodeInTree(Tree->Right, Node)));
    return Result;
}

bool IsLeafNode(node *Node)
{
    bool Result = Node->Left == NULL && Node->Right == NULL;
    return Result;
}

node *GetFirstLeafNode(node *Tree)
{
    node *Node = Tree;
    while(Node->Left)
    {
        Node = Node->Left;
    }

    return Node;
}

node *GetLastLeafNode(node *Tree)
{
    node *Node = Tree;
    while(Node->Right)
    {
        Node = Node->Right;
    }

    return Node;
}

node *GetFirstMinDepthLeafNode(node *Tree)
{
    std::queue<node *> Queue;
    Queue.push(Tree);

    while(!Queue.empty())
    {
        node *Node = Queue.front();
        Queue.pop();

        if(IsLeafNode(Node))
        {
            return Node;
        }

        Queue.push(Node->Left);
        Queue.push(Node->Right);
    }

    /* NOTE(koekeishiya): Unreachable return;
     * the binary-tree is always proper.
     * Silence compiler warning.. */
    return NULL;
}

node *GetNearestNodeToTheRight(node *Node)
{
    node *Parent = Node->Parent;
    if(Parent)
    {
        if(Parent->Right == Node)
        {
            return GetNearestNodeToTheRight(Parent);
        }

        if(IsLeafNode(Parent->Right))
        {
            return Parent->Right;
        }

        Parent = Parent->Right;
        while(!IsLeafNode(Parent->Left))
        {
            Parent = Parent->Left;
        }

        return Parent->Left;
    }

    return NULL;
}

node *GetLowestCommonAncestor(node *A, node *B)
{
    std::map<node *, bool> Ancestors;

    while(A)
    {
        Ancestors[A] = true;
        A = A->Parent;
    }

    while(B)
    {
        if(Ancestors.find(B) != Ancestors.end())
        {
            return B;
        }

        B = B->Parent;
    }

    return NULL;
}

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
equalize_node EqualizeNodeTree(node *Tree)
{
    if(IsLeafNode(Tree))
    {
        return { 1,
                 Tree->Parent ? Tree->Parent->Split == Split_Horizontal
                              : 0
               };
    }

    equalize_node LeftLeafs = EqualizeNodeTree(Tree->Left);
    equalize_node RightLeafs = EqualizeNodeTree(Tree->Right);
    equalize_node TotalLeafs = LeftLeafs + RightLeafs;

    if(Tree->Split == Split_Vertical)
    {
        int Max = MAX(LeftLeafs.VerticalCount, RightLeafs.VerticalCount);

        int SubTotal = ceil((float)TotalLeafs.HorizontalCount / 2);
        int TotalLeafsVertical = TotalLeafs.VerticalCount - SubTotal;

        if(Max == LeftLeafs.VerticalCount)
        {
            if(LeftLeafs.VerticalCount == LeftLeafs.HorizontalCount)
            {
                Max = LeftLeafs.VerticalCount = 1;
                LeftLeafs.HorizontalCount = 0;
                TotalLeafs = LeftLeafs + RightLeafs;
                SubTotal = ceil((float)TotalLeafs.HorizontalCount / 2);
                TotalLeafsVertical = TotalLeafs.VerticalCount - SubTotal;
            }

            int SubLeft = ceil((float)LeftLeafs.HorizontalCount / 2);
            float Ratio = (float) (Max - SubLeft) / TotalLeafsVertical;
            Tree->Ratio = Ratio;
        }
        else
        {
            if(RightLeafs.VerticalCount == RightLeafs.HorizontalCount)
            {
                Max = RightLeafs.VerticalCount = 1;
                RightLeafs.HorizontalCount = 0;
                TotalLeafs = LeftLeafs + RightLeafs;
                SubTotal = ceil((float)TotalLeafs.HorizontalCount / 2);
                TotalLeafsVertical = TotalLeafs.VerticalCount - SubTotal;
            }

            int SubRight = ceil((float)RightLeafs.HorizontalCount / 2);
            float Ratio = (float) (Max - SubRight) / TotalLeafsVertical;
            Tree->Ratio = 1 - Ratio;
        }
    }
    else
    {
        int Min = MIN(LeftLeafs.HorizontalCount, RightLeafs.HorizontalCount);
        if(!Min)
        {
            if(LeftLeafs.HorizontalCount == RightLeafs.HorizontalCount)
            {
                ++TotalLeafs.HorizontalCount;
            }

            ++TotalLeafs.HorizontalCount;
        }

        int Max = MAX(LeftLeafs.HorizontalCount, RightLeafs.HorizontalCount);
        if(!Max)
        {
            if(LeftLeafs.HorizontalCount == RightLeafs.HorizontalCount)
            {
                Max = LeftLeafs.HorizontalCount = 1;
            }
            else if(Max == LeftLeafs.HorizontalCount)
            {
                Max = LeftLeafs.HorizontalCount = 1;
            }
            else if(Max == RightLeafs.HorizontalCount)
            {
                Max = 1;
            }
        }

        float Ratio = (float) Max / TotalLeafs.HorizontalCount;
        if(Max == LeftLeafs.HorizontalCount)
        {
            Tree->Ratio = Ratio;
        }
        else
        {
            Tree->Ratio = 1 - Ratio;
        }
    }

    return TotalLeafs;
}

node *GetNodeWithId(node *Tree, uint32_t WindowId, virtual_space_mode VirtualSpaceMode)
{
    node *Node = GetFirstLeafNode(Tree);
    while(Node)
    {
        if(Node->WindowId == WindowId)
        {
            return Node;
        }
        else if(VirtualSpaceMode == Virtual_Space_Bsp)
        {
            Node = GetNearestNodeToTheRight(Node);
        }
        else if(VirtualSpaceMode == Virtual_Space_Monocle)
        {
            Node = Node->Right;
        }
    }

    return NULL;
}

void SwapNodeIds(node *A, node *B)
{
    uint32_t TempId = A->WindowId;
    A->WindowId = B->WindowId;
    B->WindowId = TempId;
}
