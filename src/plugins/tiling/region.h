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
struct preselect_node;
struct macos_space;
struct virtual_space;

region CGRectToRegion(CGRect Rect);
region RoundPreselRegion(region Region, CGPoint Position, CGSize Size);
void ConstrainRegion(CFStringRef DisplayRef, region *Region);
region FullscreenRegion(CFStringRef DisplayRef, virtual_space *VirtualSpace);

void CreateNodeRegion(node *Node, region_type Type, macos_space *Space, virtual_space *VirtualSpace);
void CreateNodeRegionRecursive(node *Node, bool Optimal, macos_space *Space, virtual_space *VirtualSpace);

void CreatePreselectRegion(preselect_node *Preselect, region_type Type, macos_space *Space, virtual_space *VirtualSpace);

void ResizeNodeRegion(node *Node, macos_space *Space, virtual_space *VirtualSpace);

#endif
