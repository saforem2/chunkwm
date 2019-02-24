#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <AvailabilityMacros.h>

#include <map>
#include <vector>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/cgeventtap.h"
#include "../../common/config/cvar.h"
#include "../../common/config/tokenize.h"
#include "../../common/ipc/daemon.h"
#include "../../common/misc/carbon.h"
#include "../../common/misc/workspace.h"
#include "../../common/misc/assert.h"
#include "../../common/misc/profile.h"
#include "../../common/border/border.h"

#include "../../common/accessibility/display.mm"
#include "../../common/accessibility/application.cpp"
#include "../../common/accessibility/window.cpp"
#include "../../common/accessibility/element.cpp"
#include "../../common/accessibility/observer.cpp"
#include "../../common/dispatch/cgeventtap.cpp"
#include "../../common/config/cvar.cpp"
#include "../../common/config/tokenize.cpp"
#include "../../common/ipc/daemon.cpp"
#include "../../common/misc/carbon.cpp"
#include "../../common/misc/workspace.mm"
#include "../../common/border/border.mm"

#include "presel.h"
#include "config.h"
#include "region.h"
#include "node.h"
#include "vspace.h"
#include "controller.h"
#include "rule.h"
#include "mouse.h"
#include "constants.h"
#include "misc.h"

extern chunkwm_log *c_log;

#include "presel.mm"
#include "config.cpp"
#include "region.cpp"
#include "node.cpp"
#include "vspace.cpp"
#include "controller.cpp"
#include "rule.cpp"
#include "mouse.cpp"

#define internal static
#define local_persist static

inline bool
operator!=(const CGPoint& lhs, const CGPoint& rhs)
{
    return (lhs.x != rhs.x) || (lhs.y != rhs.y);
}

inline bool
operator!=(const CGSize& lhs, const CGSize& rhs)
{
    return (lhs.width != rhs.width) || (lhs.height != rhs.height);
}

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

#define CGSDefaultConnection _CGSDefaultConnection()
typedef int CGSConnectionID;
extern "C" CGSConnectionID _CGSDefaultConnection(void);
extern "C" CGError CGSGetOnScreenWindowCount(const CGSConnectionID CID, CGSConnectionID TID, int *Count);
extern "C" CGError CGSGetOnScreenWindowList(const CGSConnectionID CID, CGSConnectionID TID, int Count, int *List, int *OutCount);

internal const char *PluginName = "Tiling";
internal const char *PluginVersion = "0.3.15";

internal macos_application_map Applications;
internal macos_window_map Windows;
internal pthread_mutex_t WindowsLock;
internal event_tap EventTap;
internal chunkwm_api API;
chunkwm_log *c_log;

#if 0
/*
 * NOTE(koekeishiya): Signals chunkwm to unload and load the tiling plugin.
 * This will only work if the "plugin_dir" cvar has been set in the config!
 */
internal void Reload()
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 3920)) {
        char Message[64];
        sprintf(Message, "core::unload tiling.so");
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);

    if (ConnectToDaemon(&SockFD, 3920)) {
        char Message[64];
        sprintf(Message, "core::load tiling.so");
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}
#endif

macos_window_map CopyWindowCache()
{
    pthread_mutex_lock(&WindowsLock);
    macos_window_map Copy = Windows;
    pthread_mutex_unlock(&WindowsLock);
    return Copy;
}

internal void
FadeWindows(uint32_t FocusedWindowId)
{
    float Alpha = CVarFloatingPointValue(CVAR_WINDOW_FADE_ALPHA);
    float Duration = CVarFloatingPointValue(CVAR_WINDOW_FADE_DURATION);
    macos_window_map Copy = CopyWindowCache();

    for (macos_window_map_it It = Copy.begin(); It != Copy.end(); ++It) {
        macos_window *Window = It->second;
        if (!AXLibHasFlags(Window, Rule_Alpha_Changed)) {
            if (Window->Id == FocusedWindowId) {
                ExtendedDockSetWindowAlpha(FocusedWindowId, 1.0f, Duration);
            } else {
                ExtendedDockSetWindowAlpha(Window->Id, Alpha, Duration);
            }
        }
    }
}

/*
 * NOTE(koekeishiya): We need a way to retrieve AXUIElementRef from a CGWindowID.
 * There is no way to do this, without caching AXUIElementRef references.
 * Here we perform a lookup of macos_window structs.
 */
internal inline macos_window *
_GetWindowByID(uint32_t Id)
{
    macos_window_map_it It = Windows.find(Id);
    return It != Windows.end() ? It->second : NULL;
}

macos_window *GetWindowByID(uint32_t Id)
{
    pthread_mutex_lock(&WindowsLock);
    macos_window *Result = _GetWindowByID(Id);
    pthread_mutex_unlock(&WindowsLock);
    return Result;
}

uint32_t GetFocusedWindowId()
{
    AXUIElementRef ApplicationRef, WindowRef;
    uint32_t WindowId = 0;

    ApplicationRef = AXLibGetFocusedApplication();
    if (!ApplicationRef) goto out;

    WindowRef = AXLibGetFocusedWindow(ApplicationRef);
    if (!WindowRef) goto err;

    WindowId = AXLibGetWindowID(WindowRef);
    CFRelease(WindowRef);

err:
    CFRelease(ApplicationRef);

out:
    return WindowId;
}

macos_window *GetFocusedWindow()
{
    uint32_t WindowId = GetFocusedWindowId();
    return WindowId ? GetWindowByID(WindowId) : NULL;
}

// NOTE(koekeishiya): Caller is responsible for making sure that the window is not a dupe.
internal void
AddWindowToCollection(macos_window *Window)
{
    if (!Window->Id) return;
    pthread_mutex_lock(&WindowsLock);
    Windows[Window->Id] = Window;
    pthread_mutex_unlock(&WindowsLock);
    ApplyRulesForWindow(Window);
}

internal macos_window *
RemoveWindowFromCollection(macos_window *Window)
{
    pthread_mutex_lock(&WindowsLock);
    macos_window *Result = _GetWindowByID(Window->Id);
    if (Result) {
        Windows.erase(Window->Id);
    }
    pthread_mutex_unlock(&WindowsLock);
    return Result;
}

internal void
ClearWindowCache()
{
    pthread_mutex_lock(&WindowsLock);
    for (macos_window_map_it It = Windows.begin(); It != Windows.end(); ++It) {
        macos_window *Window = It->second;
        AXLibDestroyWindow(Window);
    }
    Windows.clear();
    pthread_mutex_unlock(&WindowsLock);
}

internal void
AddApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if (!WindowList) {
        return;
    }

    macos_window **List = WindowList;
    macos_window *Window;
    while ((Window = *List++)) {
        if (GetWindowByID(Window->Id)) {
            AXLibDestroyWindow(Window);
        } else {
            AddWindowToCollection(Window);
        }
    }

    free(WindowList);
}

internal void
UpdateWindowCollection()
{
    for (macos_application_map_it It = Applications.begin(); It != Applications.end(); ++It) {
        macos_application *Application = It->second;
        AddApplicationWindowList(Application);
    }
}

internal void
AddApplication(macos_application *Application)
{
    Applications[Application->PID] = Application;
}

internal void
RemoveApplication(macos_application *Application)
{
    macos_application_map_it It = Applications.find(Application->PID);
    if (It != Applications.end()) {
        macos_application *Copy = It->second;
        AXLibDestroyApplication(Copy);
        Applications.erase(Application->PID);
    }
}

internal void
ClearApplicationCache()
{
    for (macos_application_map_it It = Applications.begin(); It != Applications.end(); ++It) {
        macos_application *Application = It->second;
        AXLibDestroyApplication(Application);
    }
    Applications.clear();
}

internal void
BroadcastFocusedWindowFloating()
{
    uint32_t Data[2] = { 0, 0 };
    API.Broadcast(PluginName, "focused_window_float", (char *) Data, sizeof(Data));
}

void BroadcastFocusedWindowFloating(macos_window *Window)
{
    uint32_t Status = (uint32_t) AXLibHasFlags(Window, Window_Float);
    uint32_t Data[2] = { Window->Id, Status };
    API.Broadcast(PluginName, "focused_window_float", (char *) Data, sizeof(Data));
}

void BroadcastFocusedDesktopMode(virtual_space *VirtualSpace)
{
    char *Data = virtual_space_mode_str[VirtualSpace->Mode];
    API.Broadcast(PluginName, "focused_desktop_mode", (char *) Data, strlen(Data) + 1);
}

bool IsWindowValid(macos_window *Window)
{
    bool Result;
    if (AXLibHasFlags(Window, Window_Invalid)) {
        Result = false;
    } else if (AXLibHasFlags(Window, Window_ForceTile)) {
        Result = true;
    } else {
        Result = ((AXLibIsWindowStandard(Window)) &&
                  (AXLibHasFlags(Window, Window_Movable)) &&
                  (AXLibHasFlags(Window, Window_Resizable)));
    }
    return Result;
}

internal bool
IsWindowFocusable(macos_window *Window)
{
    bool Result = ((AXLibIsWindowStandard(Window) ||
                    AXLibHasFlags(Window, Window_ForceTile)) &&
                   (!AXLibHasFlags(Window, Window_Invalid)));
    return Result;
}

internal bool
TileWindowPreValidation(macos_window *Window)
{
    if (AXLibHasFlags(Window, Window_Float)) {
        return false;
    }

    if (!IsWindowValid(Window)) {
        FloatWindow(Window);
        return false;
    }

    if (CVarIntegerValue(CVAR_WINDOW_FLOAT_NEXT)) {
        FloatWindow(Window);
        UpdateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);
        return false;
    }

    return true;
}

// NOTE(koekeishiya): Caller is responsible for making sure that the window is a valid window
// that we can properly manage. The given macos_space must also be of type kCGSSpaceUser,
// meaning that it is a space we can legally interact with.
void TileWindowOnSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    CFStringRef DisplayRef;

    if (VirtualSpace->Mode == Virtual_Space_Float) {
        goto out;
    }

    /*
     * NOTE(koekeishiya): This function appears to always return a valid identifier!
     * Could this potentially return NULL if an invalid CGSSpaceID is passed ?
     * The function returns NULL if "Displays have separate spaces" is disabled !!!
     */
    DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if (AXLibIsDisplayChangingSpaces(DisplayRef)) {
        goto display_free;
    }

    if (VirtualSpace->Tree) {
        node *Exists = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if (Exists) {
            goto display_free;
        }

        node *Node = NULL;
        uint32_t InsertionPoint = CVarUnsignedValue(CVAR_BSP_INSERTION_POINT);
        if (!InsertionPoint) {
            InsertionPoint = CVarUnsignedValue(CVAR_FOCUSED_WINDOW);
        }

        if (VirtualSpace->Mode == Virtual_Space_Bsp) {

            /*
             * NOTE(koekeishiya): When a new window is being tiled, the following priority is taking place.
             *
             *              1. If the desktop has an active preselection, the window is placed here.
             *              2. If there are any pending pseudo-leafs (layout deserialization), fill this region.
             *              3. If the focused window is eligible, split this region.
             *              4. Find the first minimum-depth leaf node and split this region.
             */

            if (VirtualSpace->Preselect) {
                CreateLeafNodePairPreselect(VirtualSpace->Preselect->Node,
                                            VirtualSpace->Preselect->Node->WindowId,
                                            Window->Id, Space, VirtualSpace);
                ApplyNodeRegion(VirtualSpace->Preselect->Node, VirtualSpace->Mode);
                FreePreselectNode(VirtualSpace);
            } else {
                Node = GetFirstMinDepthPseudoLeafNode(VirtualSpace->Tree);
                if (Node) {
                    if (Node->Parent) {
                        int SpawnLeft = CVarIntegerValue(CVAR_BSP_SPAWN_LEFT);
                        node_ids NodeIds = AssignNodeIds(Node->Parent->WindowId, Window->Id, SpawnLeft);
                        Node->Parent->WindowId = Node_Root;
                        Node->Parent->Left->WindowId = NodeIds.Left;
                        Node->Parent->Right->WindowId = NodeIds.Right;
                        CreateNodeRegionRecursive(Node->Parent, false, Space, VirtualSpace);
                        ApplyNodeRegion(Node->Parent, VirtualSpace->Mode);
                    } else {
                        Node->WindowId = Window->Id;
                        CreateNodeRegion(Node, Region_Full, Space, VirtualSpace);
                        ApplyNodeRegion(Node, VirtualSpace->Mode);
                    }
                    goto display_free;
                }

                if (InsertionPoint) {
                    Node = GetNodeWithId(VirtualSpace->Tree, InsertionPoint, VirtualSpace->Mode);
                }

                if (!Node) {
                    Node = GetFirstMinDepthLeafNode(VirtualSpace->Tree);
                    ASSERT(Node != NULL);
                }

                node_split Split = NodeSplitFromString(CVarStringValue(CVAR_BSP_SPLIT_MODE));
                if (Split == Split_Optimal) {
                    Split = OptimalSplitMode(Node);
                }

                CreateLeafNodePair(Node, Node->WindowId, Window->Id, Split, Space, VirtualSpace);
                ApplyNodeRegion(Node, VirtualSpace->Mode);
            }

            // NOTE(koekeishiya): Reset fullscreen-zoom state.
            if (VirtualSpace->Tree->Zoom) {
                VirtualSpace->Tree->Zoom = NULL;
            }
        } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
            if (InsertionPoint) {
                Node = GetNodeWithId(VirtualSpace->Tree, InsertionPoint, VirtualSpace->Mode);
            }

            if (!Node) {
                Node = GetLastLeafNode(VirtualSpace->Tree);
                ASSERT(Node != NULL);
            }

            node *NewNode = CreateRootNode(Window->Id, Space, VirtualSpace);

            if (Node->Right) {
                node *Next = Node->Right;
                Next->Left = NewNode;
                NewNode->Right = Next;
            }

            NewNode->Left = Node;
            Node->Right = NewNode;
            ResizeWindowToRegionSize(NewNode);
        }
    } else {
        char *Buffer;
        if ((ShouldDeserializeVirtualSpace(VirtualSpace)) &&
            ((Buffer = ReadFile(VirtualSpace->TreeLayout)))) {
            VirtualSpace->Tree = DeserializeNodeFromBuffer(Buffer);
            VirtualSpace->Tree->WindowId = Window->Id;
            CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
            CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
            ResizeWindowToRegionSize(VirtualSpace->Tree);
            free(Buffer);
        } else {
            // NOTE(koekeishiya): This path is equal for both bsp and monocle spaces!
            VirtualSpace->Tree = CreateRootNode(Window->Id, Space, VirtualSpace);
            ResizeWindowToRegionSize(VirtualSpace->Tree);
        }
    }

display_free:
    CFRelease(DisplayRef);
out:;
}

void TileWindow(macos_window *Window)
{
    if (TileWindowPreValidation(Window)) {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if (Space->Type == kCGSSpaceUser) {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            TileWindowOnSpace(Window, Space, VirtualSpace);
            ReleaseVirtualSpace(VirtualSpace);
        }

        AXLibDestroySpace(Space);
    }
}

internal bool
UntileWindowPreValidation(macos_window *Window)
{
    if (AXLibHasFlags(Window, Window_Float)) {
        return false;
    }

    if (!IsWindowValid(Window)) {
        return false;
    }

    return true;
}

// NOTE(koekeishiya): We need a way to identify a node in our virtualspaces by window id only.
// This is required to make sure that RebalanceWindowTree works properly.
// See https://github.com/koekeishiya/chunkwm/issues/66 for history.
internal void
UntileWindowFromSpace(uint32_t WindowId, macos_space *Space, virtual_space *VirtualSpace)
{
    if ((!VirtualSpace->Tree) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        return;
    }

    node *Node = GetNodeWithId(VirtualSpace->Tree, WindowId, VirtualSpace->Mode);
    if (!Node) {
        return;
    }

    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        /*
         * NOTE(koekeishiya): The window was in fullscreen-zoom.
         * We need to null the pointer to prevent a potential bug.
         */
        if (VirtualSpace->Tree->Zoom == Node) {
            VirtualSpace->Tree->Zoom = NULL;
        }

        if (Node->Parent && Node->Parent->Left && Node->Parent->Right) {
            /*
             * NOTE(koekeishiya): The window was in parent-zoom.
             * We need to null the pointer to prevent a potential bug.
             */
            if (Node->Parent->Zoom == Node) {
                Node->Parent->Zoom = NULL;
            }

            node *NewLeaf = Node->Parent;
            node *RemainingLeaf = IsRightChild(Node) ? Node->Parent->Left
                                                     : Node->Parent->Right;
            NewLeaf->Left = NULL;
            NewLeaf->Right = NULL;
            NewLeaf->Zoom = NULL;

            NewLeaf->WindowId = RemainingLeaf->WindowId;
            if (RemainingLeaf->Left && RemainingLeaf->Right) {
                NewLeaf->Left = RemainingLeaf->Left;
                NewLeaf->Left->Parent = NewLeaf;

                NewLeaf->Right = RemainingLeaf->Right;
                NewLeaf->Right->Parent = NewLeaf;

                CreateNodeRegionRecursive(NewLeaf, true, Space, VirtualSpace);
            }

            /*
             * NOTE(koekeishiya): Re-zoom window after spawned window closes.
             * see reference: https://github.com/koekeishiya/chunkwm/issues/20
             */
            ApplyNodeRegion(NewLeaf, VirtualSpace->Mode);
            if (NewLeaf->Parent && NewLeaf->Parent->Zoom) {
                ResizeWindowToExternalRegionSize(NewLeaf->Parent->Zoom,
                                                 NewLeaf->Parent->Region);
            }

            FreeNode(RemainingLeaf);
            FreeNode(Node);
        } else if (!Node->Parent) {
            FreeNode(VirtualSpace->Tree);
            VirtualSpace->Tree = NULL;
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        node *Prev = Node->Left;
        node *Next = Node->Right;

        if (Prev) {
            Prev->Right = Next;
        }

        if (Next) {
            Next->Left = Prev;
        }

        if (Node == VirtualSpace->Tree) {
            VirtualSpace->Tree = Next;
        }

        FreeNode(Node);
    }
}

// NOTE(koekeishiya): Caller is responsible for making sure that the window is a valid window
// that we can properly manage. The given macos_space must also be of type kCGSSpaceUser,
// meaning that it is a space we can legally interact with.
void UntileWindowFromSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    UntileWindowFromSpace(Window->Id, Space, VirtualSpace);
}

void UntileWindow(macos_window *Window)
{
    if (UntileWindowPreValidation(Window)) {
        __AppleGetDisplayIdentifierFromMacOSWindow(Window);
        ASSERT(DisplayRef);

        macos_space *Space = AXLibActiveSpace(DisplayRef);
        ASSERT(Space);

        if (Space->Type == kCGSSpaceUser) {
            virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
            UntileWindowFromSpace(Window, Space, VirtualSpace);
            ReleaseVirtualSpace(VirtualSpace);
        }

        AXLibDestroySpace(Space);
        __AppleFreeDisplayIdentifierFromWindow();
    }
}

/* NOTE(koekeishiya): Returns a vector of CGWindowIDs. */
std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space, bool IncludeInvalidWindows, bool IncludeFloatingWindows)
{
    BEGIN_TIMED_BLOCK();
    std::vector<uint32_t> Result;

    int WindowCount;
    int *WindowList = AXLibSpaceWindows(Space->Id, &WindowCount);
    if (WindowList) {
        unsigned DesktopId;
        bool Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
        ASSERT(Success);

        for (int Index = 0; Index < WindowCount; ++Index) {
            uint32_t WindowId = WindowList[Index];

            macos_window *Window = GetWindowByID(WindowId);
            if (!Window) {
                // NOTE(koekeishiya): The chunkwm core does not report these windows to
                // plugins, and they are therefore never cached, we simply ignore them.
                // DEBUG_PRINT("   %d:window not cached\n", WindowId);
                continue;
            }

            if (IsWindowValid(Window) || IncludeInvalidWindows) {
                c_log(C_LOG_LEVEL_DEBUG,
                      "%d:desktop   %d:%d:%s:%s\n",
                      DesktopId,
                      Window->Id,
                      Window->Level,
                      Window->Owner->Name,
                      Window->Name);
                if ((!AXLibHasFlags(Window, Window_Float)) || (IncludeFloatingWindows)) {
                    Result.push_back(Window->Id);
                }
            } else {
                c_log(C_LOG_LEVEL_DEBUG,
                      "%d:desktop   %d:%d:invalid window:%s:%s\n",
                      DesktopId,
                      Window->Id,
                      Window->Level,
                      Window->Owner->Name,
                      Window->Name);
            }
        }

        free(WindowList);
    }

    END_TIMED_BLOCK();
    return Result;
}

std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space)
{
    return GetAllVisibleWindowsForSpace(Space, false, false);
}

internal std::vector<uint32_t>
GetAllWindowsInTree(node *Tree, virtual_space_mode VirtualSpaceMode)
{
    std::vector<uint32_t> Windows;

    node *Node = GetFirstLeafNode(Tree);
    while (Node) {
        if (IsLeafNode(Node)) {
            Windows.push_back(Node->WindowId);
        }

        if (VirtualSpaceMode == Virtual_Space_Bsp) {
            Node = GetNextLeafNode(Node);
        } else if (VirtualSpaceMode == Virtual_Space_Monocle) {
            Node = Node->Right;
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowsToAddToTree(std::vector<uint32_t> &VisibleWindows, std::vector<uint32_t> &WindowsInTree)
{
    std::vector<uint32_t> Windows;
    for (size_t WindowIndex = 0; WindowIndex < VisibleWindows.size(); ++WindowIndex) {
        bool Found = false;
        uint32_t WindowId = VisibleWindows[WindowIndex];

        for (size_t Index = 0; Index < WindowsInTree.size(); ++Index) {
            if (WindowId == WindowsInTree[Index]) {
                Found = true;
                break;
            }
        }

        if ((!Found) && (!AXLibStickyWindow(WindowId))) {
            Windows.push_back(WindowId);
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowsToRemoveFromTree(std::vector<uint32_t> &VisibleWindows, std::vector<uint32_t> &WindowsInTree)
{
    std::vector<uint32_t> Windows;
    for (size_t Index = 0; Index < WindowsInTree.size(); ++Index) {
        bool Found = false;
        uint32_t WindowId = WindowsInTree[Index];

        for (size_t WindowIndex = 0; WindowIndex < VisibleWindows.size(); ++WindowIndex) {
            if (VisibleWindows[WindowIndex] == WindowId) {
                Found = true;
                break;
            }
        }

        if (!Found) {
            Windows.push_back(WindowsInTree[Index]);
        }
    }

    return Windows;
}

/*
 * NOTE(koekeishiya): The caller is responsible for making sure that the space
 * passed to this function is of type kCGSSpaceUser, and that the virtual space
 * is set to a tiling mode, and that an existing tree is not present. The window
 * list must also be non-empty !!!
 */
internal void
CreateWindowTreeForSpaceWithWindows(macos_space *Space, virtual_space *VirtualSpace, std::vector<uint32_t> Windows)
{
    node *New, *Root = CreateRootNode(Windows[0], Space, VirtualSpace);
    VirtualSpace->Tree = Root;

    if (VirtualSpace->Mode == Virtual_Space_Bsp) {
        for (size_t Index = 1; Index < Windows.size(); ++Index) {
            New = GetFirstMinDepthLeafNode(Root);
            ASSERT(New != NULL);

            node_split Split = NodeSplitFromString(CVarStringValue(CVAR_BSP_SPLIT_MODE));
            if (Split == Split_Optimal) {
                Split = OptimalSplitMode(New);
            }

            CreateLeafNodePair(New, New->WindowId, Windows[Index], Split, Space, VirtualSpace);
        }
    } else if (VirtualSpace->Mode == Virtual_Space_Monocle) {
        for (size_t Index = 1; Index < Windows.size(); ++Index) {
            New = CreateRootNode(Windows[Index], Space, VirtualSpace);
            Root->Right = New;
            New->Left = Root;
            Root = New;
        }
    }

    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);
}

/*
 * NOTE(koekeishiya): The caller is responsible for making sure that the space
 * passed to this function is of type kCGSSpaceUser, and that the virtual space
 * is set to bsp tiling mode. The window list must also be non-empty !!!
 */
internal void
CreateDeserializedWindowTreeForSpaceWithWindows(macos_space *Space, virtual_space *VirtualSpace, std::vector<uint32_t> Windows)
{
    if (!VirtualSpace->Tree) {
        char *Buffer = ReadFile(VirtualSpace->TreeLayout);
        if (Buffer) {
            VirtualSpace->Tree = DeserializeNodeFromBuffer(Buffer);
            free(Buffer);
        } else {
            c_log(C_LOG_LEVEL_ERROR, "failed to open '%s' for reading!\n", VirtualSpace->TreeLayout);
            CreateWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
            return;
        }
    }

    node *Root = VirtualSpace->Tree;
    for (size_t Index = 0; Index < Windows.size(); ++Index) {
        node *Node = GetFirstMinDepthPseudoLeafNode(Root);
        if (Node) {
            if (Node->Parent) {
                // NOTE(koekeishiya): This is an intermediate leaf node in the tree.
                // We simulate the process of performing a new split, but use the
                // existing node configuration.
                int SpawnLeft = CVarIntegerValue(CVAR_BSP_SPAWN_LEFT);
                node_ids NodeIds = AssignNodeIds(Node->Parent->WindowId, Windows[Index], SpawnLeft);
                Node->Parent->WindowId = Node_Root;
                Node->Parent->Left->WindowId = NodeIds.Left;
                Node->Parent->Right->WindowId = NodeIds.Right;
            } else {
                // NOTE(koekeishiya): This is the root node, we temporarily
                // use it as a leaf node, even though it really isn't.
                Node->WindowId = Windows[Index];
            }
        } else {
            // NOTE(koekeishiya): There are more windows than containers in the layout
            // We perform a regular split with node creation.
            Node = GetFirstMinDepthLeafNode(Root);
            ASSERT(Node != NULL);

            node_split Split = NodeSplitFromString(CVarStringValue(CVAR_BSP_SPLIT_MODE));
            if (Split == Split_Optimal) {
                Split = OptimalSplitMode(Node);
            }

            CreateLeafNodePair(Node, Node->WindowId, Windows[Index], Split, Space, VirtualSpace);
        }
    }

    CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
    CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
    ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
}

void CreateWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace)
{
    std::vector<uint32_t> Windows;

    if ((VirtualSpace->Tree) ||
        (VirtualSpace->Mode == Virtual_Space_Float)) {
        return;
    }

    Windows = GetAllVisibleWindowsForSpace(Space);
    if (Windows.empty()) {
        return;
    }

    CreateWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
}

void CreateDeserializedWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace)
{
    if (VirtualSpace->Mode != Virtual_Space_Bsp) {
        return;
    }

    std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space);
    if (Windows.empty()) {
        return;
    }

    CreateDeserializedWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
}

void CreateWindowTree()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    /*
     * NOTE(koekeishiya): This function appears to always return a valid identifier!
     * Could this potentially return NULL if an invalid CGSSpaceID is passed ?
     * The function returns NULL if "Displays have separate spaces" is disabled !!!
     */
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if (AXLibIsDisplayChangingSpaces(DisplayRef)) {
        goto space_free;
    }

    if (Space->Type == kCGSSpaceUser) {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if (ShouldDeserializeVirtualSpace(VirtualSpace)) {
            CreateDeserializedWindowTreeForSpace(Space, VirtualSpace);
        } else {
            CreateWindowTreeForSpace(Space, VirtualSpace);
        }
        ReleaseVirtualSpace(VirtualSpace);
    }

space_free:
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

/*
 * NOTE(koekeishiya): The caller is responsible for making sure that the space
 * passed to this function is of type kCGSSpaceUser, and that the virtual space
 * is set to a tiling mode, and that an existing tree is present. The window list
 * must also be non-empty !!!
 */
internal void
RebalanceWindowTreeForSpaceWithWindows(macos_space *Space, virtual_space *VirtualSpace, std::vector<uint32_t> Windows)
{
    std::vector<uint32_t> WindowsInTree = GetAllWindowsInTree(VirtualSpace->Tree, VirtualSpace->Mode);
    std::vector<uint32_t> WindowsToAdd = GetAllWindowsToAddToTree(Windows, WindowsInTree);
    std::vector<uint32_t> WindowsToRemove = GetAllWindowsToRemoveFromTree(Windows, WindowsInTree);

    for (size_t Index = 0; Index < WindowsToRemove.size(); ++Index) {
        macos_window *Window = GetWindowByID(WindowsToRemove[Index]);
        if ((Window) && (UntileWindowPreValidation(Window))) {
            UntileWindowFromSpace(Window, Space, VirtualSpace);
        } else {
            UntileWindowFromSpace(WindowsToRemove[Index], Space, VirtualSpace);
        }
    }

    for (size_t Index = 0; Index < WindowsToAdd.size(); ++Index) {
        macos_window *Window = GetWindowByID(WindowsToAdd[Index]);
        if ((Window) && (TileWindowPreValidation(Window))) {
            TileWindowOnSpace(Window, Space, VirtualSpace);
        }
    }
}

internal void
RebalanceWindowTreeForSpace(macos_space *Space, virtual_space *VirtualSpace)
{
    std::vector<uint32_t> Windows;

    if ((!VirtualSpace->Tree) ||
        (VirtualSpace->Mode == Virtual_Space_Float)) {
        return;
    }

    Windows = GetAllVisibleWindowsForSpace(Space);

    /*
     * NOTE(koekeishiya): We need to rebalacne our window-tree even though
     * there are no visible windows left on this desktop.
     *
     * In short; When we quit the last application we trigger an Application_Terminated
     * event, and this event relies on RebalanceWindowTree to properly restore the window.
     *
     * See https://github.com/koekeishiya/chunkwm/issues/69 for history.
     */

    RebalanceWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
}

internal void
RebalanceWindowTree()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    /*
     * NOTE(koekeishiya): This function appears to always return a valid identifier!
     * Could this potentially return NULL if an invalid CGSSpaceID is passed ?
     * The function returns NULL if "Displays have separate spaces" is disabled !!!
     */
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if (AXLibIsDisplayChangingSpaces(DisplayRef)) {
        goto space_free;
    }

    if (Space->Type == kCGSSpaceUser) {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        RebalanceWindowTreeForSpace(Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

space_free:
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

internal void
WindowFocusedHandler(uint32_t WindowId)
{
    uint32_t FocusedWindowId = CVarUnsignedValue(CVAR_FOCUSED_WINDOW);
    UpdateCVar(CVAR_FOCUSED_WINDOW, WindowId);
    macos_window *Window = GetWindowByID(WindowId);
    if (Window && IsWindowFocusable(Window)) {
        __AppleGetDisplayIdentifierFromMacOSWindow(Window);
        ASSERT(DisplayRef);

        macos_space *Space = AXLibActiveSpace(DisplayRef);
        ASSERT(Space);

        if (!AXLibSpaceHasWindow(Space->Id, Window->Id)) {
            goto space_free;
        }

        if (RuleChangedDesktop(Window->Flags)) {
            AXLibClearFlags(Window, Rule_Desktop_Changed);
            goto space_free;
        }

        if (CVarIntegerValue(CVAR_WINDOW_FADE_INACTIVE)) {
            FadeWindows(WindowId);
        }

        if ((FocusedWindowId != WindowId) &&
            (StringEquals(CVarStringValue(CVAR_MOUSE_FOLLOWS_FOCUS), Mouse_Follows_Focus_All))) {
            CenterMouseInWindow(Window);
        }

        BroadcastFocusedWindowFloating(Window);

space_free:

        AXLibDestroySpace(Space);
        __AppleFreeDisplayIdentifierFromWindow();
    }
}

internal void
ApplicationActivatedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if (WindowRef) {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        CFRelease(WindowRef);
        WindowFocusedHandler(WindowId);
    }
}

internal void
ApplicationLaunchedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if (WindowList) {
        macos_window **List = WindowList;
        macos_window *Window;
        while ((Window = *List++)) {
            if (GetWindowByID(Window->Id)) {
                AXLibDestroyWindow(Window);
            } else {
                AddWindowToCollection(Window);
                if (RuleChangedDesktop(Window->Flags)) continue;
                if (RuleTiledWindow(Window->Flags))    continue;
                TileWindow(Window);
            }
        }

        free(WindowList);
    }
}

internal void
ApplicationTerminatedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    RemoveApplication(Application);
    RebalanceWindowTree();
}

internal void
ApplicationHiddenHandler(void *Data)
{
    // macos_application *Application = (macos_application *) Data;
    RebalanceWindowTree();
}

internal void
ApplicationUnhiddenHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_space *Space;
    macos_window *Window, **List, **WindowList;

    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    List = WindowList = AXLibWindowListForApplication(Application);
    if (!WindowList) {
        goto space_free;
    }

    while ((Window = *List++)) {
        macos_window *Copy = RemoveWindowFromCollection(Window);
        if (Copy) {
            AXLibDestroyWindow(Copy);
        }

        AddWindowToCollection(Window);

        if (Window && AXLibSpaceHasWindow(Space->Id, Window->Id)) {
            TileWindow(Window);
        }
    }

    free(WindowList);

space_free:
    AXLibDestroySpace(Space);
}

internal void
WindowCreatedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);
    if (RuleChangedDesktop(Copy->Flags)) return;
    if (RuleTiledWindow(Copy->Flags))    return;
    TileWindow(Copy);
}

internal void
WindowSheetCreatedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);
    FloatWindow(Copy);
}

internal void
WindowDestroyedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = RemoveWindowFromCollection(Window);
    if (Copy) {
        if (AXLibHasFlags(Copy, Window_Float)) {
            unsigned FocusedWindowId = CVarUnsignedValue(CVAR_FOCUSED_WINDOW);
            if (Copy->Id == FocusedWindowId) {
                BroadcastFocusedWindowFloating();
            }
        }

        if (IsWindowValid(Copy)) {
            UntileWindow(Copy);
        } else {
            RebalanceWindowTree();
            uint32_t FocusedWindow = GetFocusedWindowId();
            if (FocusedWindow) {
                UpdateCVar(CVAR_FOCUSED_WINDOW, FocusedWindow);
            }
        }

        AXLibDestroyWindow(Copy);
    } else {
        // NOTE(koekeishiya): Due to unknown reasons, copy returns null for
        // some windows that we receive a destroyed event for, in particular
        // this happens a lot with Steam, what the.. ??
        UntileWindow(Window);
    }
}

internal void
WindowMinimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    ASSERT(Copy);

    UntileWindow(Copy);
}

internal void
WindowDeminimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if ((Space->Type == kCGSSpaceUser) &&
        (AXLibSpaceHasWindow(Space->Id, Window->Id))) {
        macos_window *Copy = GetWindowByID(Window->Id);
        ASSERT(Copy);

        if (AXLibHasFlags(Copy, Window_Init_Minimized)) {
            if (Copy->Mainrole) {
                CFRelease(Copy->Mainrole);
                AXLibGetWindowRole(Copy->Ref, &Copy->Mainrole);
            }

            if (Copy->Subrole) {
                CFRelease(Copy->Subrole);
                AXLibGetWindowSubrole(Copy->Ref, &Copy->Subrole);
            }
            AXLibClearFlags(Copy, Window_Init_Minimized);
        }

        TileWindow(Copy);
    }

    AXLibDestroySpace(Space);
}

internal void
WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    WindowFocusedHandler(Window->Id);
}

internal void
WindowMovedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    macos_window *Copy = GetWindowByID(Window->Id);
    if (Copy) {
        if (Copy->Position != Window->Position) {
            Copy->Position = Window->Position;
            if (CVarIntegerValue(CVAR_WINDOW_REGION_LOCKED)) {
                ConstrainWindowToRegion(Copy);
            }
        }
    }
}

internal void
WindowResizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    if (Copy) {
        if ((Copy->Position != Window->Position) ||
            (Copy->Size != Window->Size)) {
            Copy->Position = Window->Position;
            Copy->Size = Window->Size;

            if (CVarIntegerValue(CVAR_WINDOW_REGION_LOCKED)) {
                ConstrainWindowToRegion(Copy);
            }
        }
    }
}

internal void
WindowTitleChangedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    if (Copy) {
        if (Copy->Name) {
            free(Copy->Name);
        }

        Copy->Name = strdup(Window->Name);
        ApplyRulesForWindowOnTitleChanged(Copy);
    }
}

internal void
SpaceAndDisplayChangedHandler(void *Data)
{
    unsigned DesktopId, CachedDesktopId;
    std::vector<uint32_t> Windows;
    virtual_space *VirtualSpace;
    macos_space *Space;
    uint32_t WindowId;
    bool Success;

    UpdateWindowCollection();

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);

    CachedDesktopId = CVarIntegerValue(CVAR_ACTIVE_DESKTOP);
    if (CachedDesktopId != DesktopId) {
        UpdateCVar(CVAR_LAST_ACTIVE_DESKTOP, (int)CachedDesktopId);
        UpdateCVar(CVAR_ACTIVE_DESKTOP, (int)DesktopId);
    }

    VirtualSpace = AcquireVirtualSpace(Space);
    Windows = GetAllVisibleWindowsForSpace(Space);

    if (!Windows.empty()) {
        if (VirtualSpace->Mode != Virtual_Space_Float) {
            if (VirtualSpace->Tree) {
                //
                // NOTE(koekeishiya): If the activated virtual_space is flagged for resize,
                // we update the dimensions of all existing nodes in our window-tree.
                //
                if (VirtualSpaceHasFlags(VirtualSpace, Virtual_Space_Require_Resize)) {
                    VirtualSpaceRecreateRegions(Space, VirtualSpace);
                }

                //
                // NOTE(koekeishiya): If the activated virtual_space is flagged for region update,
                // we re-apply all the region of all existing nodes in our window-tree.
                //
                if (VirtualSpaceHasFlags(VirtualSpace, Virtual_Space_Require_Region_Update)) {
                    VirtualSpaceUpdateRegions(VirtualSpace);
                }

                RebalanceWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
            } else if (ShouldDeserializeVirtualSpace(VirtualSpace)) {
                CreateDeserializedWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
            } else {
                CreateWindowTreeForSpaceWithWindows(Space, VirtualSpace, Windows);
            }
        }
    }

    BroadcastFocusedDesktopMode(VirtualSpace);
    ReleaseVirtualSpace(VirtualSpace);

space_free:
    AXLibDestroySpace(Space);

    WindowId = GetFocusedWindowId();
    if (WindowId) {
        WindowFocusedHandler(WindowId);
    }
}

internal void
RecreateVirtualSpaceRegionsForDisplay(CFStringRef DisplayRef)
{
    macos_space *ActiveSpace = AXLibActiveSpace(DisplayRef);
    ASSERT(ActiveSpace);

    macos_space *Space, **List, **Spaces;
    List = Spaces = AXLibSpacesForDisplay(DisplayRef);
    ASSERT(Spaces);

    while ((Space = *List++)) {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        ASSERT(VirtualSpace);

        if (VirtualSpace->Tree) {
            if (Space->Id == ActiveSpace->Id) {
                //
                // NOTE(koekeishiya): Update dimensions of the currently active desktop
                // for the monitor that triggered a resolution change.
                //
                CreateNodeRegion(VirtualSpace->Tree, Region_Full, Space, VirtualSpace);
                CreateNodeRegionRecursive(VirtualSpace->Tree, false, Space, VirtualSpace);
                ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode, false);
            } else {
                //
                // NOTE(koekeishiya): We can not update dimensions of windows that are on inactive desktops,
                // so we flag them for change upon next activation instead
                //
                VirtualSpaceAddFlags(VirtualSpace, Virtual_Space_Require_Resize);
            }
        }

        ReleaseVirtualSpace(VirtualSpace);
        AXLibDestroySpace(Space);
    }

    AXLibDestroySpace(ActiveSpace);
    free(Spaces);
}

internal void
DisplayResizedHandler(void *Data)
{
    CGDirectDisplayID DisplayId = *(CGDirectDisplayID *) Data;
    CFStringRef DisplayRef = AXLibGetDisplayIdentifier(DisplayId);
    ASSERT(DisplayRef);

    RecreateVirtualSpaceRegionsForDisplay(DisplayRef);
    CFRelease(DisplayRef);
}

internal void
DisplayMovedHandler(void *Data)
{
    CFStringRef DisplayRef;
    CGDirectDisplayID DisplayId = *(CGDirectDisplayID *) Data;
    CGDirectDisplayID MainDisplayId = CGMainDisplayID();

    if (DisplayId == MainDisplayId) {
        DisplayRef = AXLibGetDisplayIdentifier(MainDisplayId);
        ASSERT(DisplayRef);
        RecreateVirtualSpaceRegionsForDisplay(DisplayRef);
        CFRelease(DisplayRef);
    } else {
        DisplayRef = AXLibGetDisplayIdentifier(MainDisplayId);
        ASSERT(DisplayRef);
        RecreateVirtualSpaceRegionsForDisplay(DisplayRef);
        CFRelease(DisplayRef);
        DisplayRef = AXLibGetDisplayIdentifier(DisplayId);
        ASSERT(DisplayRef);
        RecreateVirtualSpaceRegionsForDisplay(DisplayRef);
        CFRelease(DisplayRef);
    }
}

#if 0
internal void
DisplayAddedHandler(void *Data)
{
    /*
     * NOTE(koekeishiya): Temporary workaround to reload upon display-changes.
     *   Reload();
     *                    So..  this workaround does not actually work, because
     *                    we seem to get these notifications when a macOS wakes from sleep..
     */

    CGDirectDisplayID DisplayId = *(CGDirectDisplayID *) Data;
    CFStringRef DisplayRef = AXLibGetDisplayIdentifier(DisplayId);
    if (DisplayRef) {

        /*
         * TODO(koekeishiya): we need to cache all relevant information,
         * using display_id as our lookup-key.
         *
         * the end-goal is to restore the state of all virtual_spaces belonging to
         * this monitor, assuming that this monitor has been connected once before
         * in the current session.
         */

        char *Ref = CopyCFStringToC(DisplayRef);
        printf("display with ref '%s' connected!\n", Ref);
        free(Ref);
        CFRelease(DisplayRef);

        /*
         * TODO(koekeishiya): basically what we want to do when a monitor is connected.
         *
         * pending_display_state *state = NULL;
         * if ((state = PendingDisplayState(DisplayId))) {
         *     for each window_state .. state.window_state {
         *         window = window_state.window;
         *         space_id = window_state.original_space_id;
         *
         *         for each space .. spaces_for_window(window) {
         *             untile_window(window, space);
         *         }
         *
         *         original_space = space_from_id(space_id);
         *         tile_window(window, original_space);
         *     }
         * }
         */

    } else {

        /*
         * NOTE(koekeishiya): well.. if we get here, we are screwed.
         */

        printf("display with id '%u' connected (could not retireve ref)\n", DisplayId);
    }
}

internal void
DisplayRemovedHandler(void *Data)
{
    /*
     * NOTE(koekeishiya): Temporary workaround to reload upon display-changes.
     *   Reload();
     *                    So..  this workaround does not actually work, because
     *                    we seem to get these notifications when a macOS wakes from sleep..
     */
    CGDirectDisplayID DisplayId = *(CGDirectDisplayID *) Data;
    CFStringRef DisplayRef = AXLibGetDisplayIdentifier(DisplayId);
    if (DisplayRef) {

        /*
         * NOTE(koekeishiya): macOS has already cleared its internal state for this monitor,
         * and we are left empty-handed. Really appreciated, amazing.
         *
         * this branch will never run, but what can you do.
         */

        char *Ref = CopyCFStringToC(DisplayRef);
        printf("display with ref '%s' disconnected!\n", Ref);
        free(Ref);
        CFRelease(DisplayRef);
    } else {

        /*
         * TODO(koekeishiya): we need to store our current state of all window-trees that could
         * be impacted by this monitor disconnected event. Hopefully, we get this notification
         * BEFORE macOS starts to relocate windows and desktops to their target monitor. if not..
         *
         * the end-goal is to store enough information that we will be able to properly track
         * precisely the windows that will will be duped in our system, such that this operation
         * can be undone the next time this monitor is connected and macOS performs a restore.
         */

        printf("display with id '%u' disconnected (could not retireve ref)\n", DisplayId);

        /*
         * TODO(koekeishiya): basically what we want to do when a monitor is disconnected.
         *
         * display_cache *info = SearchDisplayCache(DisplayId);
         * if (info) {
         *     // add pending changes here; don't even know..
         * } else {
         *     // well.. that's not good
         * }
         */
    }
}
#endif

internal void
ChunkwmDaemonCommandHandler(void *Data)
{
    chunkwm_payload *Payload = (chunkwm_payload *) Data;
    CommandCallback(Payload->SockFD, Payload->Command, Payload->Message);
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if (StringEquals(Node, "chunkwm_export_application_launched")) {
        ApplicationLaunchedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_terminated")) {
        ApplicationTerminatedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_hidden")) {
        ApplicationHiddenHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_unhidden")) {
        ApplicationUnhiddenHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_activated")) {
        ApplicationActivatedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_created")) {
        WindowCreatedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_destroyed")) {
        WindowDestroyedHandler(Data);
        return true;
    } else if(StringEquals(Node, "chunkwm_export_window_minimized")) {
        WindowMinimizedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_deminimized")) {
        WindowDeminimizedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_focused")) {
        WindowFocusedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_moved")) {
        WindowMovedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_resized")) {
        WindowResizedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_sheet_created")) {
        WindowSheetCreatedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_title_changed")) {
        WindowTitleChangedHandler(Data);
        return true;
    } else if ((StringEquals(Node, "chunkwm_export_space_changed")) ||
               (StringEquals(Node, "chunkwm_export_display_changed"))) {
        SpaceAndDisplayChangedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_display_resized")) {
        DisplayResizedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_display_moved")) {
        DisplayMovedHandler(Data);
        return true;
#if 0
    } else if (StringEquals(Node, "chunkwm_export_display_added")) {
        DisplayAddedHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_display_removed")) {
        DisplayRemovedHandler(Data);
        return true;
#endif
    } else if (StringEquals(Node, "chunkwm_daemon_command")) {
        ChunkwmDaemonCommandHandler(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_events_subscribed")) {
        /* NOTE(koekeishiya): Tile windows visible on the current space using configured mode */
        CreateWindowTree();

        /* NOTE(koekeishiya): Set our initial insertion-point on launch. */
        uint32_t WindowId = GetFocusedWindowId();
        if (WindowId) {
            if (CVarIntegerValue(CVAR_WINDOW_FADE_INACTIVE)) {
                FadeWindows(WindowId);
            }
            WindowFocusedHandler(WindowId);
        }
    }

    return false;
}

internal bool
Init(chunkwm_api ChunkwmAPI)
{
    bool Success;
    std::vector<macos_application *> Applications;
    uint32_t ProcessPolicy;

    macos_space *Space;
    unsigned DesktopId;

    API = ChunkwmAPI;
    c_log = API.Log;
    BeginCVars(&API);

    Success = (pthread_mutex_init(&WindowsLock, NULL) == 0);
    if (!Success) goto out;

    CreateCVar(CVAR_SPACE_MODE, virtual_space_mode_str[Virtual_Space_Bsp]);

    CreateCVar(CVAR_BAR_ENABLED, 0);
    CreateCVar(CVAR_BAR_ALL_MONITORS, 0);
    CreateCVar(CVAR_BAR_OFFSET_TOP, 22.0f);
    CreateCVar(CVAR_BAR_OFFSET_BOTTOM, 0.0f);
    CreateCVar(CVAR_BAR_OFFSET_LEFT, 0.0f);
    CreateCVar(CVAR_BAR_OFFSET_RIGHT, 0.0f);

    CreateCVar(CVAR_SPACE_OFFSET_TOP, 60.0f);
    CreateCVar(CVAR_SPACE_OFFSET_BOTTOM, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_LEFT, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_RIGHT, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_GAP, 20.0f);

    CreateCVar(CVAR_PADDING_STEP_SIZE, 10.0f);
    CreateCVar(CVAR_GAP_STEP_SIZE, 5.0f);

    CreateCVar(CVAR_FOCUSED_WINDOW, 0);
    CreateCVar(CVAR_BSP_INSERTION_POINT, 0);

    CreateCVar(CVAR_ACTIVE_DESKTOP, 0);
    CreateCVar(CVAR_LAST_ACTIVE_DESKTOP, 0);

    CreateCVar(CVAR_BSP_SPAWN_LEFT, 1);
    CreateCVar(CVAR_BSP_OPTIMAL_RATIO, 1.618f);
    CreateCVar(CVAR_BSP_SPLIT_RATIO, 0.5f);
    CreateCVar(CVAR_BSP_SPLIT_MODE, node_split_str[Split_Optimal]);

    CreateCVar(CVAR_MONITOR_FOCUS_CYCLE, 0);
    CreateCVar(CVAR_WINDOW_FOCUS_CYCLE, Window_Focus_Cycle_None);

    CreateCVar(CVAR_MOUSE_FOLLOWS_FOCUS, Mouse_Follows_Focus_Off);
    CreateCVar(CVAR_MOUSE_MOVE_BINDING, Mouse_Move_Binding);
    CreateCVar(CVAR_MOUSE_RESIZE_BINDING, Mouse_Resize_Binding);
    CreateCVar(CVAR_MOUSE_MOTION_INTERVAL, 35.0f);

    CreateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);
    CreateCVar(CVAR_WINDOW_REGION_LOCKED, 0);

    CreateCVar(CVAR_PRE_BORDER_COLOR, 0xffffff00);
    CreateCVar(CVAR_PRE_BORDER_WIDTH, 4);
    CreateCVar(CVAR_PRE_BORDER_RADIUS, 4);
    CreateCVar(CVAR_PRE_BORDER_OUTLINE, 0);

    /*
     * NOTE(koekeishiya): The following cvars requires extended dock
     * functionality provided by chwm-sa to work.
     */

    CreateCVar(CVAR_WINDOW_FLOAT_TOPMOST, 0);
    CreateCVar(CVAR_WINDOW_FADE_INACTIVE, 0);
    CreateCVar(CVAR_WINDOW_FADE_ALPHA, 0.85f);
    CreateCVar(CVAR_WINDOW_FADE_DURATION, 0.5f);
    CreateCVar(CVAR_WINDOW_CGS_MOVE, 0);

    /*   ---------------------------------------------------------   */

    CGEnableEventStateCombining(false);

    ProcessPolicy = Process_Policy_Regular | Process_Policy_LSUIElement;
    Applications = AXLibRunningProcesses(ProcessPolicy);
    for (size_t Index = 0; Index < Applications.size(); ++Index) {
        macos_application *Application = Applications[Index];
        AddApplication(Application);
        AddApplicationWindowList(Application);
    }

    Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);

    AXLibDestroySpace(Space);

    UpdateCVar(CVAR_ACTIVE_DESKTOP, (int)DesktopId);
    UpdateCVar(CVAR_LAST_ACTIVE_DESKTOP, (int)DesktopId);

    Success = BeginVirtualSpaces();
    if (Success) {
        bool MouseMoveBound = BindMouseMoveAction(CVarStringValue(CVAR_MOUSE_MOVE_BINDING));
        bool MouseResizeBound = BindMouseResizeAction(CVarStringValue(CVAR_MOUSE_RESIZE_BINDING));
        if (MouseMoveBound || MouseResizeBound) {
            EventTap.Mask = ((1 << kCGEventLeftMouseDown) |
                             (1 << kCGEventLeftMouseDragged) |
                             (1 << kCGEventLeftMouseUp) |
                             (1 << kCGEventRightMouseDown) |
                             (1 << kCGEventRightMouseDragged) |
                             (1 << kCGEventRightMouseUp));
            BeginEventTap(&EventTap, &EventTapCallback);
        }
        goto out;
    }

    c_log(C_LOG_LEVEL_ERROR, "chunkwm-tiling: failed to initialize virtual space system!\n");

    EndEventTap(&EventTap);
    ClearApplicationCache();
    ClearWindowCache();

out:
    return Success;
}

internal void
Deinit()
{
    EndEventTap(&EventTap);

    ClearApplicationCache();
    ClearWindowCache();
    FreeWindowRules();

    EndVirtualSpaces();
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    return Init(ChunkwmAPI);
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    Deinit();
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_launched,
    chunkwm_export_application_terminated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,
    chunkwm_export_application_activated,

    chunkwm_export_window_created,
    chunkwm_export_window_destroyed,
    chunkwm_export_window_minimized,
    chunkwm_export_window_deminimized,
    chunkwm_export_window_focused,

    chunkwm_export_window_moved,
    chunkwm_export_window_resized,
    chunkwm_export_window_title_changed,
    chunkwm_export_window_sheet_created,

    chunkwm_export_space_changed,
    chunkwm_export_display_changed,

    chunkwm_export_display_resized,
    chunkwm_export_display_moved,

#if 0
    chunkwm_export_display_added,
    chunkwm_export_display_removed,
#endif
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN(PluginName, PluginVersion)
