#include "node.h"
#include "vspace.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"

#include <queue>

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

node_split OptimalSplitMode(node *Node)
{
    float OptimalRatio = CVarFloatingPointValue("bsp_optimal_ratio");

    float NodeRatio = Node->Region.Width / Node->Region.Height;
    return NodeRatio >= OptimalRatio ? Split_Vertical : Split_Horizontal;
}

node *CreateRootNode(macos_display *Display)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->Ratio = CVarFloatingPointValue("bsp_split_ratio");
    Node->Split = Split_None;
    CreateNodeRegion(Display, Node, Region_Full);

    return Node;
}

node *CreateLeafNode(macos_display *Display, node *Parent, uint32_t WindowId, region_type Type)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->WindowId = WindowId;
    Node->Parent = Parent;
    CreateNodeRegion(Display, Node, Type);

    return Node;
}

void CreateLeafNodePair(macos_display *Display, node *Parent,
                        uint32_t ExistingWindowId, uint32_t SpawnedWindowId, node_split Split)
{
    Parent->WindowId = 0;
    Parent->Split = Split;
    Parent->Ratio = CVarFloatingPointValue("bsp_split_ratio");

    uint32_t LeftWindowId, RightWindowId;
    if(CVarIntegerValue("bsp_spawn_left"))
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
        Parent->Left = CreateLeafNode(Display, Parent, LeftWindowId, Region_Left);
        Parent->Right = CreateLeafNode(Display, Parent, RightWindowId, Region_Right);
    }
    else if(Split == Split_Horizontal)
    {
        Parent->Left = CreateLeafNode(Display, Parent, LeftWindowId, Region_Upper);
        Parent->Right = CreateLeafNode(Display, Parent, RightWindowId, Region_Lower);
    }
}

void ResizeWindowToRegionSize(node *Node)
{
    // NOTE(koekeishiya): GetWindowByID should not be able to fail!
    macos_window *Window = GetWindowByID(Node->WindowId);
    ASSERT(Window);

    AXLibSetWindowPosition(Window->Ref, Node->Region.X, Node->Region.Y);
    AXLibSetWindowSize(Window->Ref, Node->Region.Width, Node->Region.Height);
}

void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode)
{
    if(Node->WindowId)
    {
        ResizeWindowToRegionSize(Node);
    }

    if(Node->Left && VirtualSpaceMode == Virtual_Space_Bsp)
    {
        ApplyNodeRegion(Node->Left, VirtualSpaceMode);
    }

    if(Node->Right)
    {
        ApplyNodeRegion(Node->Right, VirtualSpaceMode);
    }
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

node *GetFirstMinDepthLeafNode(node *Root)
{
    std::queue<node *> Queue;
    Queue.push(Root);

    while(!Queue.empty())
    {
        node *Node = Queue.front();
        Queue.pop();

        if(!Node->Left && !Node->Right)
            return Node;

        if(Node->Left)
            Queue.push(Node->Left);

        if(Node->Right)
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
