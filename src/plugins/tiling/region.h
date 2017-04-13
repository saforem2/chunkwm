#ifndef PLUGIN_REGION_H
#define PLUGIN_REGION_H

enum region_type
{
    Region_Full = 0,

    Region_Left = 1,
    Region_Right = 2,

    Region_Upper = 3,
    Region_Lower = 4,
};

struct region
{
    float X, Y;
    float Width, Height;
    region_type Type;
};

struct region_offset
{
    float Top, Bottom;
    float Left, Right;
    float Gap;
};

struct node;

void ConstrainRegion(region *Region);

void CreateNodeRegion(node *Node, region_type Type);
void CreateNodeRegionRecursive(node *Node, bool Optimal);

void ResizeNodeRegion(node *Node);

#endif
