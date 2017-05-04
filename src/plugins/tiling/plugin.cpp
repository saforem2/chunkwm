#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>

#include <map>
#include <vector>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/ipc/daemon.h"
#include "../../common/misc/carbon.h"
#include "../../common/misc/assert.h"
#include "../../common/config/cvar.h"

#include "config.h"
#include "region.h"
#include "node.h"
#include "vspace.h"
#include "controller.h"
#include "constants.h"
#include "misc.h"

#define internal static
#define local_persist static

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

#define CGSDefaultConnection _CGSDefaultConnection()
typedef int CGSConnectionID;
extern "C" CGSConnectionID _CGSDefaultConnection(void);
extern "C" CGError CGSGetOnScreenWindowCount(const CGSConnectionID CID, CGSConnectionID TID, int *Count);
extern "C" CGError CGSGetOnScreenWindowList(const CGSConnectionID CID, CGSConnectionID TID, int Count, int *List, int *OutCount);

// TODO(koekeishiya): Shorter name.
#define CONFIG_FILE "/.chunkwmtilingrc"

internal const char *PluginName = "Tiling";
internal const char *PluginVersion = "0.0.2";

internal macos_application_map Applications;
internal macos_window_map Windows;

plugin_broadcast *ChunkWMBroadcastEvent;

/* NOTE(koekeishiya): We need a way to retrieve AXUIElementRef from a CGWindowID.
 * There is no way to do this, without caching AXUIElementRef references.
 * Here we perform a lookup of macos_window structs. */
macos_window *GetWindowByID(uint32_t Id)
{
    macos_window_map_it It = Windows.find(Id);
    return It != Windows.end() ? It->second : NULL;
}

// NOTE(koekeishiya): Caller is responsible for making sure that the window is not a dupe.
internal void
AddWindowToCollection(macos_window *Window)
{
    Windows[Window->Id] = Window;
}

internal macos_window *
RemoveWindowFromCollection(macos_window *Window)
{
    macos_window *Result = GetWindowByID(Window->Id);
    if(Result)
    {
        Windows.erase(Window->Id);
    }
    return Result;
}

internal void
AddApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if(WindowList)
    {
        macos_window **List = WindowList;
        macos_window *Window;
        while((Window = *List++))
        {
            if(GetWindowByID(Window->Id))
            {
                AXLibDestroyWindow(Window);
            }
            else
            {
                AddWindowToCollection(Window);
            }
        }

        free(WindowList);
    }
}

internal void
UpdateWindowCollection()
{
    for(macos_application_map_it It = Applications.begin();
        It != Applications.end();
        ++It)
    {
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
    if(It != Applications.end())
    {
        macos_application *Copy = It->second;
        AXLibDestroyApplication(Copy);

        Applications.erase(Application->PID);
    }
}

bool IsWindowValid(macos_window *Window)
{
    bool Result = ((AXLibIsWindowStandard(Window)) &&
                   (AXLibHasFlags(Window, Window_Movable)) &&
                   (AXLibHasFlags(Window, Window_Resizable)) &&
                   (!AXLibHasFlags(Window, Window_Invalid)));
    return Result;
}

void TileWindowOnSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    /* NOTE(koekeishiya): This function appears to always return a valid identifier!
     * Could this potentially return NULL if an invalid CGSSpaceID is passed ? */
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if(AXLibIsDisplayChangingSpaces(DisplayRef))
    {
        CFRelease(DisplayRef);
        return;
    }

    if(VirtualSpace->Mode != Virtual_Space_Float)
    {
        if(VirtualSpace->Tree)
        {
            node *Exists = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
            if(!Exists)
            {
                node *Node = NULL;
                uint32_t InsertionPoint = CVarIntegerValue(CVAR_BSP_INSERTION_POINT);
                if(InsertionPoint)
                {
                    Node = GetNodeWithId(VirtualSpace->Tree, InsertionPoint, VirtualSpace->Mode);
                }

                if(VirtualSpace->Mode == Virtual_Space_Bsp)
                {
                    if(!Node)
                    {
                        Node = GetFirstMinDepthLeafNode(VirtualSpace->Tree);
                        ASSERT(Node != NULL);
                    }

                    node_split Split = (node_split) CVarIntegerValue(CVAR_BSP_SPLIT_MODE);
                    if(Split == Split_Optimal)
                    {
                        Split = OptimalSplitMode(Node);
                    }

                    CreateLeafNodePair(Node, Node->WindowId, Window->Id, Split, Space, VirtualSpace);
                    ApplyNodeRegion(Node, VirtualSpace->Mode);

                    // NOTE(koekeishiya): Reset fullscreen-zoom state.
                    if(VirtualSpace->Tree->Zoom)
                    {
                        VirtualSpace->Tree->Zoom = NULL;
                    }
                }
                else if(VirtualSpace->Mode == Virtual_Space_Monocle)
                {
                    if(!Node)
                    {
                        Node = GetLastLeafNode(VirtualSpace->Tree);
                        ASSERT(Node != NULL);
                    }

                    node *NewNode = CreateRootNode(Window->Id, Space, VirtualSpace);

                    if(Node->Right)
                    {
                        node *Next = Node->Right;
                        Next->Left = NewNode;
                        NewNode->Right = Next;
                    }

                    NewNode->Left = Node;
                    Node->Right = NewNode;
                    ResizeWindowToRegionSize(NewNode);
                }
            }
        }
        else
        {
            // NOTE(koekeishiya): This path is equal for both bsp and monocle spaces!
            VirtualSpace->Tree = CreateRootNode(Window->Id, Space, VirtualSpace);
            ResizeWindowToRegionSize(VirtualSpace->Tree);
        }
    }
}

void TileWindow(macos_window *Window)
{
    if(AXLibHasFlags(Window, Window_Float))
    {
        return;
    }

    if(!IsWindowValid(Window))
    {
        FloatWindow(Window);
        return;
    }

    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_NEXT))
    {
        FloatWindow(Window);
        UpdateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);
        return;
    }

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        TileWindowOnSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibDestroySpace(Space);
}

void TileWindow(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    if(AXLibHasFlags(Window, Window_Float))
    {
        return;
    }

    if(!IsWindowValid(Window))
    {
        FloatWindow(Window);
        return;
    }

    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_NEXT))
    {
        FloatWindow(Window);
        UpdateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);
        return;
    }

    TileWindowOnSpace(Window, Space, VirtualSpace);
}

void UntileWindowFromSpace(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
    {
        node *Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
        if(Node)
        {
            if(VirtualSpace->Mode == Virtual_Space_Bsp)
            {
                /* NOTE(koekeishiya): The window was in fullscreen-zoom.
                 * We need to null the pointer to prevent a potential bug. */
                if(VirtualSpace->Tree->Zoom == Node)
                {
                    VirtualSpace->Tree->Zoom = NULL;
                }

                if(Node->Parent && Node->Parent->Left && Node->Parent->Right)
                {
                    /* NOTE(koekeishiya): The window was in parent-zoom.
                     * We need to null the pointer to prevent a potential bug. */
                    if(Node->Parent->Zoom == Node)
                    {
                        Node->Parent->Zoom = NULL;
                    }

                    node *NewLeaf = Node->Parent;
                    node *RemainingLeaf = IsRightChild(Node) ? Node->Parent->Left
                                                             : Node->Parent->Right;
                    NewLeaf->Left = NULL;
                    NewLeaf->Right = NULL;
                    NewLeaf->Zoom = NULL;

                    NewLeaf->WindowId = RemainingLeaf->WindowId;
                    if(RemainingLeaf->Left && RemainingLeaf->Right)
                    {
                        NewLeaf->Left = RemainingLeaf->Left;
                        NewLeaf->Left->Parent = NewLeaf;

                        NewLeaf->Right = RemainingLeaf->Right;
                        NewLeaf->Right->Parent = NewLeaf;

                        CreateNodeRegionRecursive(NewLeaf, true, Space, VirtualSpace);
                    }

                    /* NOTE(koekeishiya): Re-zoom window after spawned window closes.
                     * see reference: https://github.com/koekeishiya/chunkwm/issues/20 */
                    ApplyNodeRegion(NewLeaf, VirtualSpace->Mode);
                    if(NewLeaf->Parent && NewLeaf->Parent->Zoom)
                    {
                        ResizeWindowToExternalRegionSize(NewLeaf->Parent->Zoom,
                                                         NewLeaf->Parent->Region);
                    }

                    free(RemainingLeaf);
                    free(Node);
                }
                else if(!Node->Parent)
                {
                    free(VirtualSpace->Tree);
                    VirtualSpace->Tree = NULL;
                }
            }
            else if(VirtualSpace->Mode == Virtual_Space_Monocle)
            {
                node *Prev = Node->Left;
                node *Next = Node->Right;

                if(Prev)
                {
                    Prev->Right = Next;
                }

                if(Next)
                {
                    Next->Left = Prev;
                }

                if(Node == VirtualSpace->Tree)
                {
                    VirtualSpace->Tree = Next;
                }

                free(Node);
            }
        }
    }
}

void UntileWindow(macos_window *Window)
{
    if(AXLibHasFlags(Window, Window_Float))
    {
        return;
    }

    if(!IsWindowValid(Window))
    {
        return;
    }

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        UntileWindowFromSpace(Window, Space, VirtualSpace);
        ReleaseVirtualSpace(VirtualSpace);
    }

    CFRelease(DisplayRef);
    AXLibDestroySpace(Space);
}

void UntileWindow(macos_window *Window, macos_space *Space, virtual_space *VirtualSpace)
{
    if(AXLibHasFlags(Window, Window_Float))
    {
        return;
    }

    if(!IsWindowValid(Window))
    {
        return;
    }

    UntileWindowFromSpace(Window, Space, VirtualSpace);
}

/* NOTE(koekeishiya): Returns a vector of CGWindowIDs. */
std::vector<uint32_t> GetAllVisibleWindowsForSpace(macos_space *Space)
{
    std::vector<uint32_t> Windows;

    int WindowCount = 0;
    CGError Error = CGSGetOnScreenWindowCount(CGSDefaultConnection, 0, &WindowCount);
    if(Error == kCGErrorSuccess)
    {
        int WindowList[WindowCount];
        Error = CGSGetOnScreenWindowList(CGSDefaultConnection, 0, WindowCount, WindowList, &WindowCount);
        if(Error == kCGErrorSuccess)
        {
            for(int Index = 0;
                Index < WindowCount;
                ++Index)
            {
                uint32_t WindowId = WindowList[Index];

                /* NOTE(koekeishiya): The onscreenwindowlist can contain windowids
                 * that we do not care about. Check that the window in question is
                 * in our cache and on the correct monitor. */
                if((GetWindowByID(WindowId)) &&
                   (AXLibSpaceHasWindow(Space->Id, WindowId)))
                {
                    Windows.push_back(WindowList[Index]);
                }
            }
        }
    }

    return Windows;
}

/* NOTE(koekeishiya): Returns a vector of CGWindowIDs. */
std::vector<uint32_t> GetAllVisibleWindows()
{
    std::vector<uint32_t> Windows;

    int WindowCount = 0;
    CGError Error = CGSGetOnScreenWindowCount(CGSDefaultConnection, 0, &WindowCount);
    if(Error == kCGErrorSuccess)
    {
        int WindowList[WindowCount];
        Error = CGSGetOnScreenWindowList(CGSDefaultConnection, 0, WindowCount, WindowList, &WindowCount);
        if(Error == kCGErrorSuccess)
        {
            for(int Index = 0;
                Index < WindowCount;
                ++Index)
            {
                /* NOTE(koekeishiya): The onscreenwindowlist can contain windowids
                 * that we do not care about. Check that the window in question is
                 * in our cache. */
                if(GetWindowByID(WindowList[Index]))
                {
                    Windows.push_back(WindowList[Index]);
                }
            }
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowsInTree(node *Tree, virtual_space_mode VirtualSpaceMode)
{
    std::vector<uint32_t> Windows;

    node *Node = GetFirstLeafNode(Tree);
    while(Node)
    {
        if(Node->WindowId != 0)
        {
            Windows.push_back(Node->WindowId);
        }

        if(VirtualSpaceMode == Virtual_Space_Bsp)
        {
            Node = GetNearestNodeToTheRight(Node);
        }
        else if(VirtualSpaceMode == Virtual_Space_Monocle)
        {
            Node = Node->Right;
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowsToAddToTree(std::vector<uint32_t> &VisibleWindows, std::vector<uint32_t> &WindowsInTree)
{
    std::vector<uint32_t> Windows;
    for(size_t WindowIndex = 0;
        WindowIndex < VisibleWindows.size();
        ++WindowIndex)
    {
        bool Found = false;
        uint32_t WindowId = VisibleWindows[WindowIndex];

        for(size_t Index = 0;
            Index < WindowsInTree.size();
            ++Index)
        {
            if(WindowId == WindowsInTree[Index])
            {
                Found = true;
                break;
            }
        }

        if((!Found) && (!AXLibStickyWindow(WindowId)))
        {
            Windows.push_back(WindowId);
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowsToRemoveFromTree(std::vector<uint32_t> &VisibleWindows, std::vector<uint32_t> &WindowsInTree)
{
    std::vector<uint32_t> Windows;
    for(size_t Index = 0;
        Index < WindowsInTree.size();
        ++Index)
    {
        bool Found = false;
        uint32_t WindowId = WindowsInTree[Index];

        for(size_t WindowIndex = 0;
            WindowIndex < VisibleWindows.size();
            ++WindowIndex)
        {
            if(VisibleWindows[WindowIndex] == WindowId)
            {
                Found = true;
                break;
            }
        }

        if(!Found)
        {
            Windows.push_back(WindowsInTree[Index]);
        }
    }

    return Windows;
}

void CreateWindowTree()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    /* NOTE(koekeishiya): This function appears to always return a valid identifier,
     * as long as 'Displays have separate spaces' is enabled. Otherwise it returns NULL.
     * Probably returns NULL if an invalid CGSSpaceID is passed as well. */
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if(AXLibIsDisplayChangingSpaces(DisplayRef))
    {
        AXLibDestroySpace(Space);
        CFRelease(DisplayRef);
        return;
    }

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(!VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space);
            if(!Windows.empty())
            {
                node *Root = CreateRootNode(Windows[0], Space, VirtualSpace);
                VirtualSpace->Tree = Root;

                if(VirtualSpace->Mode == Virtual_Space_Bsp)
                {
                    for(size_t Index = 1;
                        Index < Windows.size();
                        ++Index)
                    {
                        node *Node = GetFirstMinDepthLeafNode(Root);
                        ASSERT(Node != NULL);

                        node_split Split = (node_split) CVarIntegerValue(CVAR_BSP_SPLIT_MODE);
                        if(Split == Split_Optimal)
                        {
                            Split = OptimalSplitMode(Node);
                        }

                        CreateLeafNodePair(Node, Node->WindowId, Windows[Index], Split, Space, VirtualSpace);
                    }
                }
                else if(VirtualSpace->Mode == Virtual_Space_Monocle)
                {
                    for(size_t Index = 1;
                        Index < Windows.size();
                        ++Index)
                    {
                        node *Next = CreateRootNode(Windows[Index], Space, VirtualSpace);
                        Root->Right = Next;
                        Next->Left = Root;
                        Root = Next;
                    }
                }

                ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);
            }
        }
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibDestroySpace(Space);
}

internal void
RebalanceWindowTree()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    /* NOTE(koekeishiya): This function appears to always return a valid identifier!
     * Could this potentially return NULL if an invalid CGSSpaceID is passed ? */
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
    ASSERT(DisplayRef);

    if(AXLibIsDisplayChangingSpaces(DisplayRef))
    {
        AXLibDestroySpace(Space);
        CFRelease(DisplayRef);
        return;
    }

    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            std::vector<uint32_t> Windows = GetAllVisibleWindowsForSpace(Space);
            std::vector<uint32_t> WindowsInTree = GetAllWindowsInTree(VirtualSpace->Tree, VirtualSpace->Mode);
            std::vector<uint32_t> WindowsToAdd = GetAllWindowsToAddToTree(Windows, WindowsInTree);
            std::vector<uint32_t> WindowsToRemove = GetAllWindowsToRemoveFromTree(Windows, WindowsInTree);

            for(size_t Index = 0;
                Index < WindowsToRemove.size();
                ++Index)
            {
                macos_window *Window = GetWindowByID(WindowsToRemove[Index]);
                if(Window)
                {
                    UntileWindow(Window, Space, VirtualSpace);
                }
            }

            for(size_t Index = 0;
                Index < WindowsToAdd.size();
                ++Index)
            {
                macos_window *Window = GetWindowByID(WindowsToAdd[Index]);
                if(Window)
                {
                    TileWindow(Window, Space, VirtualSpace);
                }
            }
        }
        ReleaseVirtualSpace(VirtualSpace);
    }

    AXLibDestroySpace(Space);
}

void ApplicationLaunchedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if(WindowList)
    {
        macos_window **List = WindowList;
        macos_window *Window;
        while((Window = *List++))
        {
            if(GetWindowByID(Window->Id))
            {
                AXLibDestroyWindow(Window);
            }
            else
            {
                AddWindowToCollection(Window);
                TileWindow(Window);
            }
        }

        free(WindowList);
    }
}

void ApplicationTerminatedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    RemoveApplication(Application);
    RebalanceWindowTree();
}

void ApplicationHiddenHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    RebalanceWindowTree();
}

void ApplicationUnhiddenHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type == kCGSSpaceUser)
    {
        macos_window **WindowList = AXLibWindowListForApplication(Application);
        if(WindowList)
        {
            macos_window **List = WindowList;
            macos_window *Window;
            while((Window = *List++))
            {
                if(GetWindowByID(Window->Id))
                {
                    if(AXLibSpaceHasWindow(Space->Id, Window->Id))
                    {
                        TileWindow(Window);
                    }
                }

                AXLibDestroyWindow(Window);
            }

            free(WindowList);
        }
    }

    AXLibDestroySpace(Space);
}

void ApplicationActivatedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        CFRelease(WindowRef);

        macos_window *Window = GetWindowByID(WindowId);
        if(Window && IsWindowValid(Window))
        {
            UpdateCVar(CVAR_FOCUSED_WINDOW, (int)Window->Id);
            if(!AXLibHasFlags(Window, Window_Float))
            {
                UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)Window->Id);

                // NOTE(koekeishiya): test global plugin broadcast system.
                int Status = 0;
                ChunkWMBroadcastEvent(PluginName, "focused_window_float", (char *) &Status, sizeof(int));
            }
            else
            {
                // NOTE(koekeishiya): test global plugin broadcast system.
                int Status = 1;
                ChunkWMBroadcastEvent(PluginName, "focused_window_float", (char *) &Status, sizeof(int));
            }
        }
    }
}

void WindowCreatedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);

    TileWindow(Copy);
}

void WindowDestroyedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = RemoveWindowFromCollection(Window);
    if(Copy)
    {
        UntileWindow(Copy);
        AXLibDestroyWindow(Copy);
    }
    else
    {
        // NOTE(koekeishiya): Due to unknown reasons, copy returns null for
        // some windows that we receive a destroyed event for, in particular
        // this happens a lot with Steam, what the.. ??
        UntileWindow(Window);
    }
}

void WindowMinimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    ASSERT(Copy);

    UntileWindow(Copy);
}

void WindowDeminimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if((Space->Type == kCGSSpaceUser) &&
       (AXLibSpaceHasWindow(Space->Id, Window->Id)))
    {
        macos_window *Copy = GetWindowByID(Window->Id);
        ASSERT(Copy);

        TileWindow(Copy);
    }

    AXLibDestroySpace(Space);
}

void WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    if(Copy && IsWindowValid(Copy))
    {
        UpdateCVar(CVAR_FOCUSED_WINDOW, (int)Copy->Id);
        if(!AXLibHasFlags(Copy, Window_Float))
        {
            UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)Copy->Id);

            // NOTE(koekeishiya): test global plugin broadcast system.
            int Status = 0;
            ChunkWMBroadcastEvent(PluginName, "focused_window_float", &Status, sizeof(int));
        }
        else
        {
            int Status = 1;
            ChunkWMBroadcastEvent(PluginName, "focused_window_float", &Status, sizeof(int));
        }
    }
}

void WindowMovedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    if(Copy)
    {
        Copy->Position = Window->Position;
    }
}

void WindowResizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    if(Copy)
    {
        Copy->Position = Window->Position;
        Copy->Size = Window->Size;
    }
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringEquals(Node, "chunkwm_export_application_launched"))
    {
        ApplicationLaunchedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_terminated"))
    {
        ApplicationTerminatedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_hidden"))
    {
        ApplicationHiddenHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_unhidden"))
    {
        ApplicationUnhiddenHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_activated"))
    {
        ApplicationActivatedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_created"))
    {
        WindowCreatedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_destroyed"))
    {
        WindowDestroyedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_minimized"))
    {
        WindowMinimizedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_deminimized"))
    {
        WindowDeminimizedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_focused"))
    {
        WindowFocusedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_moved"))
    {
        WindowMovedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_resized"))
    {
        WindowResizedHandler(Data);
        return true;
    }
    else if((StringEquals(Node, "chunkwm_export_space_changed")) ||
            (StringEquals(Node, "chunkwm_export_display_changed")))
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        unsigned DesktopId;
        Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
        ASSERT(Success);

        AXLibDestroySpace(Space);

        int CachedDesktopId = CVarIntegerValue(CVAR_ACTIVE_DESKTOP);
        if(CachedDesktopId != DesktopId)
        {
            UpdateCVar(CVAR_LAST_ACTIVE_DESKTOP, CachedDesktopId);
            UpdateCVar(CVAR_ACTIVE_DESKTOP, (int)DesktopId);
        }

        UpdateWindowCollection();
        CreateWindowTree();
        RebalanceWindowTree();
        return true;
    }

    return false;
}

internal bool
Init(plugin_broadcast *ChunkwmBroadcast)
{
    ChunkWMBroadcastEvent = ChunkwmBroadcast;

    BeginCVars();

    CreateCVar(CVAR_SPACE_MODE, Virtual_Space_Bsp);

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
    CreateCVar(CVAR_BSP_SPLIT_MODE, Split_Optimal);

    CreateCVar(CVAR_WINDOW_FOCUS_CYCLE, "none");

    CreateCVar(CVAR_MOUSE_FOLLOWS_FOCUS, 1);

    CreateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);
    CreateCVar(CVAR_WINDOW_FLOAT_CENTER, 0);

    /* NOTE(koekeishiya): The following cvars requires extended dock
     * functionality provided by chwm-sa to work. */

    CreateCVar(CVAR_WINDOW_FLOAT_TOPMOST, 1);

    /*   ---------------------------------------------------------   */

    int Port = 4131;
    StartDaemon(Port, DaemonCallback);

    const char *HomeEnv = getenv("HOME");
    if(HomeEnv)
    {
        unsigned HomeEnvLength = strlen(HomeEnv);
        unsigned ConfigFileLength = strlen(CONFIG_FILE);
        unsigned PathLength = HomeEnvLength + ConfigFileLength;

        // NOTE(koekeishiya): We don't need to store the config-file, as reloading the config
        // can be done externally by simply executing the bash script instead of sending us
        // a reload command. Stack allocation..
        char PathToConfigFile[PathLength + 1];
        PathToConfigFile[PathLength] = '\0';

        memcpy(PathToConfigFile, HomeEnv, HomeEnvLength);
        memcpy(PathToConfigFile + HomeEnvLength, CONFIG_FILE, ConfigFileLength);

        // NOTE(koekeishiya): Only try to execute the config file if it actually exists.
        struct stat Buffer;
        if(stat(PathToConfigFile, &Buffer) == 0)
        {
            // NOTE(koekeishiya): The config file is just an executable bash script!
            system(PathToConfigFile);
        }
        else
        {
            fprintf(stderr, "   tiling: config '%s' not found!\n", PathToConfigFile);
        }
    }
    else
    {
        fprintf(stderr,"    tiling: 'env HOME' not set!\n");
    }

    uint32_t ProcessPolicy = Process_Policy_Regular;
    std::vector<macos_application *> Applications = AXLibRunningProcesses(ProcessPolicy);

    for(size_t Index = 0; Index < Applications.size(); ++Index)
    {
        macos_application *Application = Applications[Index];
        AddApplication(Application);
        AddApplicationWindowList(Application);
    }

    /* NOTE(koekeishiya): Tile windows visible on the current space using binary space partitioning. */
    CreateWindowTree();

    /* NOTE(koekeishiya): Set our initial insertion-point on launch. */
    AXUIElementRef ApplicationRef = AXLibGetFocusedApplication();
    if(ApplicationRef)
    {
        AXUIElementRef WindowRef = AXLibGetFocusedWindow(ApplicationRef);
        CFRelease(ApplicationRef);

        if(WindowRef)
        {
            uint32_t WindowId = AXLibGetWindowID(WindowRef);
            CFRelease(WindowRef);

            macos_window *Window = GetWindowByID(WindowId);
            ASSERT(Window);

            if(IsWindowValid(Window))
            {
                UpdateCVar(CVAR_FOCUSED_WINDOW, (int)Window->Id);
                if(!AXLibHasFlags(Window, Window_Float))
                {
                    UpdateCVar(CVAR_BSP_INSERTION_POINT, (int)Window->Id);
                }
            }
        }
    }

    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    unsigned DesktopId = 1;
    Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);

    AXLibDestroySpace(Space);

    UpdateCVar(CVAR_ACTIVE_DESKTOP, (int)DesktopId);
    UpdateCVar(CVAR_LAST_ACTIVE_DESKTOP, (int)DesktopId);

    BeginVirtualSpaces();

    bool Result = true;
    return Result;
}

internal void
Deinit()
{
    StopDaemon();

    for(macos_application_map_it It = Applications.begin();
        It != Applications.end();
        ++It)
    {
        macos_application *Application = It->second;
        AXLibDestroyApplication(Application);
    }

    for(macos_window_map_it It = Windows.begin();
        It != Windows.end();
        ++It)
    {
        macos_window *Window = It->second;
        AXLibDestroyWindow(Window);
    }

    EndVirtualSpaces();
    Applications.clear();
    Windows.clear();

    EndCVars();
}

/*
 * NOTE(koekeishiya):
 * parameter: plugin_broadcast *Broadcast
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    return Init(Broadcast);
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    Deinit();
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
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

    chunkwm_export_space_changed,
    chunkwm_export_display_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN(PluginName, PluginVersion)
