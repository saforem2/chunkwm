#ifndef PLUGIN_VSPACE_H
#define PLUGIN_VSPACE_H

#include "region.h"

#include "../../common/misc/string.h"

#include <pthread.h>
#include <map>

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
    region_offset _Offset;

    region_offset *Offset;
    node *Tree;

    pthread_mutex_t Lock;
};

typedef std::map<const char *, virtual_space *, string_comparator> virtual_space_map;
typedef virtual_space_map::iterator virtual_space_map_it;

struct macos_space;
virtual_space *AcquireVirtualSpace(macos_space *Space);
void ReleaseVirtualSpace(virtual_space *VirtualSpace);

bool BeginVirtualSpaces();
void EndVirtualSpaces();

#endif
