#ifndef PLUGIN_REGION_H
#define PLUGIN_REGION_H

enum region_type
{
    Region_Full = 0,

    Region_Left = 1,
    Region_Right = 2,

    Region_Upper = 3,
    Region_Lower = 4,

    /* NOTE(koekeishiya): Is this a decent way to handle zoom ? */
    Region_Zoom_Full,
    Region_Zoom_Parent,
};

struct region
{
    float X, Y;
    float Width, Height;
    region_type Type;
};

struct region_offset
{
    float Left, Top;
    float Right, Bottom;
    float X, Y;
};

struct node;
struct macos_display;

void CreateNodeRegion(macos_display *Display, node *Node, region_type Type);
void CreateNodeRegionRecursive(macos_display *Display, node *Node, bool Optimal);

void ResizeNodeRegion(macos_display *Display, node *Node);

#endif
