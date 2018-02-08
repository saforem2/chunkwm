#ifndef PLUGIN_VSPACE_H
#define PLUGIN_VSPACE_H

#include "region.h"

#include "../../common/misc/string.h"
#include <stdint.h>
#include <pthread.h>
#include <map>

static char *virtual_space_mode_str[] =
{
    "bsp",
    "monocle",
    "float"
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
    char *TreeLayout;
};

enum virtual_space_flags
{
    Virtual_Space_Require_Resize = 1 << 0,
    Virtual_Space_Require_Region_Update = 1 << 1,
};

struct preselect_node;
struct virtual_space
{
    virtual_space_mode Mode;
    region_offset _Offset;

    region_offset *Offset;
    char *TreeLayout;
    node *Tree;
    uint32_t Flags;
    preselect_node *Preselect;

    pthread_mutex_t Lock;
};

typedef std::map<const char *, virtual_space *, string_comparator> virtual_space_map;
typedef virtual_space_map::iterator virtual_space_map_it;

struct macos_space;
bool ShouldDeserializeVirtualSpace(virtual_space *VirtualSpace);
bool VirtualSpaceHasFlags(virtual_space *VirtualSpace, uint32_t Flag);
void VirtualSpaceAddFlags(virtual_space *VirtualSpace, uint32_t Flag);
void VirtualSpaceClearFlags(virtual_space *VirtualSpace, uint32_t Flag);
virtual_space *AcquireVirtualSpace(macos_space *Space);
void ReleaseVirtualSpace(virtual_space *VirtualSpace);

void VirtualSpaceRecreateRegions(macos_space *Space, virtual_space *VirtualSpace);
void VirtualSpaceUpdateRegions(virtual_space *VirtualSpace);

bool BeginVirtualSpaces();
void EndVirtualSpaces();

#endif
