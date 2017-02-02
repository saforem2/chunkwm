#include "node.h"

#define internal static

node_split OptimalSplitMode(node *Node)
{
    // TODO(koekeishiya): cvar system.
    float OptimalRatio = 1.618f; // Settings.OptimalRatio;

    float NodeRatio = Node->Region.Width / Node->Region.Height;
    return NodeRatio >= OptimalRatio ? Split_Vertical : Split_Horizontal;
}
