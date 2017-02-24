#ifndef PLUGIN_NODE_H
#define PLUGIN_NODE_H

#include <stdint.h>

#include "region.h"
#include "vspace.h"

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

node_split OptimalSplitMode(node *Node);
node *CreateRootNode(uint32_t WindowId);

void CreateLeafNodePair(node *Parent, uint32_t FirstWindowID, uint32_t SecondWindowID, node_split Split);
void ApplyNodeRegion(node *Node, virtual_space_mode VirtualSpaceMode);
void ResizeWindowToRegionSize(node *Node);
void ResizeWindowToExternalRegionSize(node *Node, region Region);
void FreeNodeTree(node *Node, virtual_space_mode VirtualSpaceMode);

bool IsLeafNode(node *Node);
bool IsLeftChild(node *Node);
bool IsRightChild(node *Node);
bool IsNodeInTree(node *Tree, node *Node);

node *GetFirstLeafNode(node *Tree);
node *GetLastLeafNode(node *Tree);
node *GetFirstMinDepthLeafNode(node *Root);
node *GetLowestCommonAncestor(node *A, node *B);

node *GetNearestNodeToTheRight(node *Node);
node *GetNodeWithId(node *Tree, uint32_t WindowId, virtual_space_mode VirtualSpaceMode);

void SwapNodeIds(node *A, node *B);

#endif
