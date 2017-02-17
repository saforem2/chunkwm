#ifndef PLUGIN_VSPACE_H
#define PLUGIN_VSPACE_H

#include "region.h"

#include "../../common/misc/string.h"

#include <map>

struct virtual_space_identifier
{
    int DisplayIndex, SpaceIndex;
    bool operator<(const virtual_space_identifier &Other) const
    {
        return ((DisplayIndex < Other.DisplayIndex) ||
                ((DisplayIndex == Other.DisplayIndex) &&
                 (SpaceIndex < Other.SpaceIndex)));
    }
};

enum virtual_space_mode
{
    Virtual_Space_Bsp,
    Virtual_Space_Monocle,
    Virtual_Space_Float,
};

struct virtual_space_config
{
    virtual_space_mode Mode;
    region_offset Offset;
};

struct virtual_space
{
    virtual_space_mode Mode;
    region_offset Offset;

    node *Tree;
};

typedef std::map<virtual_space_identifier, virtual_space_config> virtual_space_config_map;
typedef virtual_space_config_map::iterator virtual_space_config_map_it;

typedef std::map<const char *, virtual_space *, string_comparator> virtual_space_map;
typedef virtual_space_map::iterator virtual_space_map_it;

struct macos_display;
struct macos_space;
virtual_space *AcquireVirtualSpace(macos_display *Display, macos_space *Space);
void ReleaseVirtualSpaces();

#endif
