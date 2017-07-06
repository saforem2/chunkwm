#ifndef PLUGIN_NODE_H
#define PLUGIN_NODE_H

#include <stdint.h>

#include "region.h"
#include "vspace.h"

struct border_window;

enum node_type
{
    Node_PseudoLeaf = -1,
    Node_Root = 0,

    Node_Serialized_Root = 1,
    Node_Serialized_Leaf = 2
};

static char *node_split_str[] =
{
    "none",
    "optimal",
    "vertical",
    "horizontal"
};
enum node_split
{
    Split_None = 0,
    Split_Optimal = 1,
    Split_Vertical = 2,
    Split_Horizontal = 3
};

struct node_ids
{
    uint32_t Left;
    uint32_t Right;
};

struct preselect_node
{
    node_split Split;
    float Ratio;
    bool SpawnLeft;

    node *Node;
    region Region;
    char *Direction;

    border_window *Border;
};

struct node
{
    uint32_t WindowId;
    node_split Split;
    float Ratio;

    node *Parent;
    node *Left;
    node *Right;

    node *Zoom;
    region Region;

    preselect_node *Preselect;
};

struct equalize_node
{
    int VerticalCount;
    int HorizontalCount;
};
inline equalize_node operator+(const equalize_node A, const equalize_node B)
{
    return { A.VerticalCount + B.VerticalCount,
             A.HorizontalCount + B.HorizontalCount };
}

node_ids AssignNodeIds(uint32_t ExistingId, uint32_t NewId, bool SpawnLeft);
node_split OptimalSplitMode(node *Node);
node_split NodeSplitFromString(char *Value);

node *CreateRootNode(uint32_t WindowId, macos_space *Space, virtual_space *VirtualSpace);
node *CreateLeafNode(node *Parent, uint32_t WindowId, region_type Type, macos_space *Space, virtual_space *VirtualSpace);
void CreateLeafNodePair(node *Parent, uint32_t ExistingWindowId, uint32_t SpawnedWindowId, node_split Split, macos_space *Space, virtual_space *VirtualSpace);
void CreateLeafNodePairPreselect(node *Parent, uint32_t ExistingWindowId, uint32_t SpawnedWindowId, macos_space *Space, virtual_space *VirtualSpace);
equalize_node EqualizeNodeTree(node *Tree);
void FreeNodeTree(node *Node, virtual_space_mode VirtualSpaceMode);
void FreePreselectNode(node *Node);
void FreeNode(node *Node);

void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode);
void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode, bool Center);

void ResizeWindowToRegionSize(node *Node);
void ResizeWindowToRegionSize(node *Node, bool Center);

void ResizeWindowToExternalRegionSize(node *Node, region Region);
void ResizeWindowToExternalRegionSize(node *Node, region Region, bool Center);

struct macos_window;
void ConstrainWindowToRegion(macos_window *Window);

bool IsLeafNode(node *Node);
bool IsLeftChild(node *Node);
bool IsRightChild(node *Node);
bool IsNodeInTree(node *Tree, node *Node);

node *GetFirstLeafNode(node *Tree);
node *GetLastLeafNode(node *Tree);
node *GetFirstMinDepthLeafNode(node *Root);
node *GetFirstMinDepthPseudoLeafNode(node *Root);
node *GetLowestCommonAncestor(node *A, node *B);

node *GetNextLeafNode(node *Node);
node *GetPrevLeafNode(node *Node);
node *GetNodeWithId(node *Tree, uint32_t WindowId, virtual_space_mode VirtualSpaceMode);

struct CGPoint;
node *GetNodeForPoint(node *Node, CGPoint *Point);

void SwapNodeIds(node *A, node *B);

char *SerializeNodeToBuffer(node *Node);
node *DeserializeNodeFromBuffer(char *Buffer);

#endif
