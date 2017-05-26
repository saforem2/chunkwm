#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../../common/config/cvar.h"
#include "../../common/config/tokenize.h"
#include "../../common/misc/assert.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"

#include <queue>
#include <map>

#define internal static

extern macos_window *GetWindowByID(uint32_t Id);

node_ids AssignNodeIds(uint32_t ExistingId, uint32_t NewId)
{
    node_ids NodeIds;
    if(CVarIntegerValue(CVAR_BSP_SPAWN_LEFT))
    {
        NodeIds.Left = NewId;
        NodeIds.Right = ExistingId;
    }
    else
    {
        NodeIds.Left = ExistingId;
        NodeIds.Right = NewId;
    }
    return NodeIds;
}

node_split OptimalSplitMode(node *Node)
{
    float OptimalRatio = CVarFloatingPointValue(CVAR_BSP_OPTIMAL_RATIO);

    float NodeRatio = Node->Region.Width / Node->Region.Height;
    return NodeRatio >= OptimalRatio ? Split_Vertical : Split_Horizontal;
}

node *CreateRootNode(uint32_t WindowId, macos_space *Space, virtual_space *VirtualSpace)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->WindowId = WindowId;
    CreateNodeRegion(Node, Region_Full, Space, VirtualSpace);
    Node->Split = OptimalSplitMode(Node);
    Node->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    return Node;
}

node *CreateLeafNode(node *Parent, uint32_t WindowId, region_type Type, macos_space *Space, virtual_space *VirtualSpace)
{
    node *Node = (node *) malloc(sizeof(node));
    memset(Node, 0, sizeof(node));

    Node->Parent = Parent;
    Node->WindowId = WindowId;
    CreateNodeRegion(Node, Type, Space, VirtualSpace);
    Node->Split = OptimalSplitMode(Node);
    Node->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    return Node;
}

void CreateLeafNodePair(node *Parent, uint32_t ExistingWindowId, uint32_t SpawnedWindowId, node_split Split, macos_space *Space, virtual_space *VirtualSpace)
{
    Parent->WindowId = Node_Root;
    Parent->Split = Split;
    Parent->Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);

    node_ids NodeIds = AssignNodeIds(ExistingWindowId, SpawnedWindowId);

    ASSERT(Split == Split_Vertical || Split == Split_Horizontal);
    if(Split == Split_Vertical)
    {
        Parent->Left = CreateLeafNode(Parent, NodeIds.Left, Region_Left, Space, VirtualSpace);
        Parent->Right = CreateLeafNode(Parent, NodeIds.Right, Region_Right, Space, VirtualSpace);
    }
    else if(Split == Split_Horizontal)
    {
        Parent->Left = CreateLeafNode(Parent, NodeIds.Left, Region_Upper, Space, VirtualSpace);
        Parent->Right = CreateLeafNode(Parent, NodeIds.Right, Region_Lower, Space, VirtualSpace);
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
    if(Node->WindowId && Node->WindowId != Node_PseudoLeaf)
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
    bool Result = Node->WindowId != Node_Root;
    return Result;
}

node *GetFirstLeafNode(node *Tree)
{
    node *Node = Tree;
    while((!IsLeafNode(Node)) &&
          (Node->Left))
    {
        Node = Node->Left;
    }

    return Node;
}

node *GetLastLeafNode(node *Tree)
{
    node *Node = Tree;
    while((!IsLeafNode(Node)) &&
          (Node->Right))
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

node *GetFirstMinDepthPseudoLeafNode(node *Tree)
{
    std::queue<node *> Queue;
    Queue.push(Tree);

    while(!Queue.empty())
    {
        node *Node = Queue.front();
        Queue.pop();

        if((IsLeafNode(Node)) &&
           (Node->WindowId == Node_PseudoLeaf))
        {
            return Node;
        }

        if(Node->Left)  Queue.push(Node->Left);
        if(Node->Right) Queue.push(Node->Right);
    }

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

equalize_node EqualizeNodeTree(node *Tree)
{
    if(IsLeafNode(Tree))
    {
        return { Tree->Parent ? Tree->Parent->Split == Split_Vertical   : 0,
                 Tree->Parent ? Tree->Parent->Split == Split_Horizontal : 0 };
    }

    equalize_node LeftLeafs = EqualizeNodeTree(Tree->Left);
    equalize_node RightLeafs = EqualizeNodeTree(Tree->Right);
    equalize_node TotalLeafs = LeftLeafs + RightLeafs;

    if(Tree->Split == Split_Vertical)
    {
        Tree->Ratio = (float) LeftLeafs.VerticalCount / TotalLeafs.VerticalCount;
        --TotalLeafs.VerticalCount;
    }
    else if(Tree->Split == Split_Horizontal)
    {
        Tree->Ratio = (float) LeftLeafs.HorizontalCount / TotalLeafs.HorizontalCount;
        --TotalLeafs.HorizontalCount;
    }

    if(Tree->Parent)
    {
        TotalLeafs.VerticalCount += Tree->Parent->Split == Split_Vertical;
        TotalLeafs.HorizontalCount += Tree->Parent->Split == Split_Horizontal;
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

// NOTE(koekeishiya): This type is only used internally
struct serialized_node
{
    int TypeId;
    char *Type;
    int Split;
    float Ratio;
    serialized_node *Next;
};

internal serialized_node *
ChainSerializedNode(serialized_node *Root, const char *NodeType, node *Node)
{
    serialized_node *SerializedNode = (serialized_node *) malloc(sizeof(serialized_node));
    memset(SerializedNode, 0, sizeof(serialized_node));

    SerializedNode->TypeId = Node_Serialized_Root;
    SerializedNode->Type = strdup(NodeType);
    SerializedNode->Split = Node->Split;
    SerializedNode->Ratio = Node->Ratio;

    Root->Next = SerializedNode;
    return SerializedNode;
}

internal serialized_node *
ChainSerializedNode(serialized_node *Root, const char *NodeType)
{
    serialized_node *SerializedNode = (serialized_node *) malloc(sizeof(serialized_node));
    memset(SerializedNode, 0, sizeof(serialized_node));

    SerializedNode->TypeId = Node_Serialized_Leaf;
    SerializedNode->Type = strdup(NodeType);

    Root->Next = SerializedNode;
    return SerializedNode;
}

internal void
DestroySerializedNode(serialized_node *Node)
{
    serialized_node *Next = Node->Next;

    free(Node->Type);
    free(Node);

    if(Next) DestroySerializedNode(Next);
}

internal serialized_node *
SerializeRootNode(node *Node, const char *NodeType, serialized_node *SerializedNode)
{
    SerializedNode = ChainSerializedNode(SerializedNode, NodeType, Node);

    SerializedNode = IsLeafNode(Node->Left)
                   ? ChainSerializedNode(SerializedNode, "left_leaf")
                   : SerializeRootNode(Node->Left, "left_root", SerializedNode);

    SerializedNode = IsLeafNode(Node->Right)
                   ? ChainSerializedNode(SerializedNode, "right_leaf")
                   : SerializeRootNode(Node->Right, "right_root", SerializedNode);

    return SerializedNode;
}

// NOTE(koekeishiya): Caller is responsible for memory
char *SerializeNodeToBuffer(node *Node)
{
    serialized_node SerializedNode = {};
    SerializeRootNode(Node, "root", &SerializedNode);

    serialized_node *Current = SerializedNode.Next;

    char *Cursor, *Buffer, *EndOfBuffer;
    size_t BufferSize = sizeof(char) * 2048;
    size_t BytesWritten = 0;

    Cursor = Buffer = (char *) malloc(BufferSize);
    EndOfBuffer = Buffer + BufferSize;

    while(Current)
    {
        if(Current->TypeId == Node_Serialized_Root)
        {
            ASSERT(Cursor < EndOfBuffer);
            BytesWritten = snprintf(Cursor, BufferSize,
                                    "%s %d %.2f\n",
                                    Current->Type,
                                    Current->Split,
                                    Current->Ratio);
            ASSERT(BytesWritten >= 0);
            Cursor += BytesWritten;
            BufferSize -= BytesWritten;
        }
        else if(Current->TypeId == Node_Serialized_Leaf)
        {
            ASSERT(Cursor < EndOfBuffer);
            BytesWritten = snprintf(Cursor, BufferSize,
                                    "%s\n", Current->Type);
            ASSERT(BytesWritten >= 0);
            Cursor += BytesWritten;
            BufferSize -= BytesWritten;
        }

        Current = Current->Next;
    }

    DestroySerializedNode(SerializedNode.Next);
    return Buffer;
}

node *DeserializeNodeFromBuffer(char *Buffer)
{
    node *Tree, *Current;
    Current = Tree = (node *) malloc(sizeof(node));
    memset(Tree, 0, sizeof(node));

    const char *Cursor = Buffer;

    token Token = GetToken(&Cursor);
    ASSERT(TokenEquals(Token, "root"));

    token Split = GetToken(&Cursor);
    token Ratio = GetToken(&Cursor);

    Tree->WindowId = Node_PseudoLeaf;
    Tree->Split = (node_split) TokenToInt(Split);
    Tree->Ratio = TokenToFloat(Ratio);

    Token = GetToken(&Cursor);
    while(Token.Length > 0)
    {
        if(TokenEquals(Token, "left_root"))
        {
            node *Left = (node *) malloc(sizeof(node));
            memset(Left, 0, sizeof(node));

            token Split = GetToken(&Cursor);
            token Ratio = GetToken(&Cursor);

            Left->WindowId = Node_PseudoLeaf;
            Left->Parent = Current;
            Left->Split = (node_split) TokenToInt(Split);
            Left->Ratio = TokenToFloat(Ratio);

            Current->Left = Left;
            Current = Left;
        }
        else if(TokenEquals(Token, "right_root"))
        {
            node *Right = (node *) malloc(sizeof(node));
            memset(Right, 0, sizeof(node));

            token Split = GetToken(&Cursor);
            token Ratio = GetToken(&Cursor);

            Right->WindowId = Node_PseudoLeaf;
            Right->Parent = Current;
            Right->Split = (node_split) TokenToInt(Split);
            Right->Ratio = TokenToFloat(Ratio);

            Current->Right = Right;
            Current = Right;
        }
        else if(TokenEquals(Token, "left_leaf"))
        {
            node *Leaf = (node *) malloc(sizeof(node));
            memset(Leaf, 0, sizeof(node));

            Leaf->WindowId = Node_PseudoLeaf;
            Leaf->Parent = Current;
            Current->Left = Leaf;
        }
        else if(TokenEquals(Token, "right_leaf"))
        {
            node *Leaf = (node *) malloc(sizeof(node));
            memset(Leaf, 0, sizeof(node));

            Leaf->WindowId = Node_PseudoLeaf;
            Leaf->Parent = Current;
            Current->Right = Leaf;

            // NOTE(koekeishiya): After parsing a right-leaf, we are done with this node
            Current = Current->Parent;
        }

        Token = GetToken(&Cursor);
    }

    return Tree;
}
