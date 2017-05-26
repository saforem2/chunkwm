#ifndef PLUGIN_NODE_H
#define PLUGIN_NODE_H

#include <stdint.h>

#include "region.h"
#include "vspace.h"

enum node_type
{
    Node_PseudoLeaf = -1,
    Node_Root = 0,

    Node_Serialized_Root = 1,
    Node_Serialized_Leaf = 2
};

enum node_split
{
    Split_None = 0,
    Split_Optimal = -1,
    Split_Vertical = 1,
    Split_Horizontal = 2
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
};

struct serialized_node
{
    int TypeId;
    char *Type;
    int Split;
    float Ratio;
    serialized_node *Next;
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

node_split OptimalSplitMode(node *Node);

node *CreateRootNode(uint32_t WindowId, macos_space *Space, virtual_space *VirtualSpace);
node *CreateLeafNode(node *Parent, uint32_t WindowId, region_type Type, macos_space *Space, virtual_space *VirtualSpace);
void CreateLeafNodePair(node *Parent, uint32_t ExistingWindowId, uint32_t SpawnedWindowId, node_split Split, macos_space *Space, virtual_space *VirtualSpace);
void FreeNodeTree(node *Node, virtual_space_mode VirtualSpaceMode);
equalize_node EqualizeNodeTree(node *Tree);

void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode);
void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode, bool Center);

void ResizeWindowToRegionSize(node *Node);
void ResizeWindowToRegionSize(node *Node, bool Center);

void ResizeWindowToExternalRegionSize(node *Node, region Region);
void ResizeWindowToExternalRegionSize(node *Node, region Region, bool Center);

bool IsLeafNode(node *Node);
bool IsLeftChild(node *Node);
bool IsRightChild(node *Node);
bool IsNodeInTree(node *Tree, node *Node);

node *GetFirstLeafNode(node *Tree);
node *GetLastLeafNode(node *Tree);
node *GetFirstMinDepthLeafNode(node *Root);
node *GetFirstMinDepthPseudoLeafNode(node *Root);
node *GetLowestCommonAncestor(node *A, node *B);

node *GetNearestNodeToTheRight(node *Node);
node *GetNodeWithId(node *Tree, uint32_t WindowId, virtual_space_mode VirtualSpaceMode);

void SwapNodeIds(node *A, node *B);

serialized_node *SerializeRootNode(node *Node, const char *NodeType, serialized_node *SerializedNode);
void DestroySerializedNode(serialized_node *Node);
char *SerializeNodeToBuffer(serialized_node *SerializedNode);
node *DeserializeNodeFromBuffer(char *Buffer);

#endif
