#include "node.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

node_split OptimalSplitMode(node *Node)
{
    // TODO(koekeishiya): cvar system.
    float OptimalRatio = 1.618f; // Settings.OptimalRatio;

    float NodeRatio = Node->Region.Width / Node->Region.Height;
    return NodeRatio >= OptimalRatio ? Split_Vertical : Split_Horizontal;
}

node *CreateRootNode(macos_display *Display)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    // TODO(koekeishiya): cvar system.
    Node->Ratio = 1.618f; //Settings.SplitRatio;

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
                        uint32_t FirstWindowId, uint32_t SecondWindowId, node_split Split)
{
    Parent->WindowId = 0;
    Parent->Split = Split;

    // TODO(koekeishiya): cvar system.
    Parent->Ratio = 0.5f; // KWMSettings.SplitRatio;

    uint32_t LeftWindowId = FirstWindowId;
    uint32_t RightWindowId = SecondWindowId;

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

    if(AXLibSetWindowPosition(Window->Ref, Node->Region.X, Node->Region.Y))
        printf("set window position!\n");
    else
        printf("could not set window position!\n");

    if(AXLibSetWindowSize(Window->Ref, Node->Region.Width, Node->Region.Height))
        printf("set window size!\n");
    else
        printf("could not set window size!\n");
}

void ApplyNodeRegion(node *Node)
{
    if(Node->WindowId)
        ResizeWindowToRegionSize(Node);

    if(Node->Left)
        ApplyNodeRegion(Node->Left);

    if(Node->Right)
        ApplyNodeRegion(Node->Right);
}

void FreeNodeTree(node *Node)
{
    if(Node->Left)
    {
        FreeNodeTree(Node->Left);
    }

    if(Node->Right)
    {
        FreeNodeTree(Node->Right);
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

node *GetNodeWithId(node *Tree, uint32_t WindowId)
{
    node *Node = GetFirstLeafNode(Tree);
    while(Node)
    {
        if(Node->WindowId == WindowId)
        {
            return Node;
        }
        else
        {
            Node = GetNearestNodeToTheRight(Node);
        }
    }

    return NULL;
}
