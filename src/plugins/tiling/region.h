#ifndef PLUGIN_REGION_H
#define PLUGIN_REGION_H

#include <CoreGraphics/CGGeometry.h>

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
struct macos_space;
struct virtual_space;

region CGRectToRegion(CGRect Rect);
void ConstrainRegion(CFStringRef DisplayRef, region *Region);

void CreateNodeRegion(node *Node, region_type Type, macos_space *Space, virtual_space *VirtualSpace);
void CreateNodeRegionRecursive(node *Node, bool Optimal, macos_space *Space, virtual_space *VirtualSpace);

void ResizeNodeRegion(node *Node, macos_space *Space, virtual_space *VirtualSpace);

#endif
