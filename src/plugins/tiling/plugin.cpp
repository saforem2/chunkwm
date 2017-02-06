#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <map>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"

#include "region.h"
#include "node.h"
#include "assert.h"

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

internal unsigned DisplayCount;
internal macos_display **DisplayList;
internal macos_display *MainDisplay;

internal macos_application_map Applications;
internal macos_window_map Windows;

/* TODO(koekeishiya): Lookup space storage using macos_space. */
internal node *Tree;

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
    Applications.erase(Application->PID);
}

#include <vector>
#include <queue>

node *FindFirstMinDepthLeafNode(node *Root)
{
    std::queue<node *> Queue;
    Queue.push(Root);

    while(!Queue.empty())
    {
        node *Node = Queue.front();
        Queue.pop();

        if(!Node->Left && !Node->Right)
            return Node;

        if(Node->Left)
            Queue.push(Node->Left);

        if(Node->Right)
            Queue.push(Node->Right);
    }

    /* NOTE(koekeishiya): Unreachable return;
     * the binary-tree is always proper.
     * Silence compiler warning.. */
    return NULL;
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

internal void
CreateWindowTree(macos_display *Display)
{
    macos_space *Space = AXLibActiveSpace(Display->Ref);
    if(Space->Type != kCGSSpaceUser)
        return;

    std::vector<uint32_t> Windows = GetAllVisibleWindows();
    if(!Windows.empty())
    {
        node *Root = CreateRootNode(Display);
        Root->WindowId = Windows[0];

        for(size_t Index = 1;
            Index < Windows.size();
            ++Index)
        {
            node *Node = FindFirstMinDepthLeafNode(Root);
            Assert(Node != NULL);

            CreateLeafNodePair(Display, Node, Node->WindowId, Windows[Index], OptimalSplitMode(Node));
        }

        Tree = Root;
        ApplyNodeRegion(Tree);
    }
}

internal region_offset Offset = { 50, 50, 70, 50, 30, 30 };
region_offset *FindSpaceOffset(CGDirectDisplayID Display, CGSSpaceID Space)
{
    return &Offset;
}

void ApplicationLaunchedHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    printf("    plugin: %s launched!\n", Application->Name);

    AddApplication(Application);
}

void ApplicationTerminatedHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    printf("    plugin: %s terminated!\n", Application->Name);

    RemoveApplication(Application);
}

void ApplicationHiddenHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    printf("    plugin: %s hidden!\n", Application->Name);
}

void ApplicationUnhiddenHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    printf("    plugin: %s unhidden!\n", Application->Name);
}

void WindowCreatedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;
    printf("    plugin: %s:%s window created\n", Window->Owner->Name, Window->Name);

    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);
}

void WindowDestroyedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;
    printf("    plugin: %s:%s window destroyed\n", Window->Owner->Name, Window->Name);

    macos_window *Copy = RemoveWindowFromCollection(Window);
    if(Copy)
    {
        AXLibDestroyWindow(Copy);
    }
}

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

/*
 * NOTE(koekeishiya): Function parameters
 * const char *Node
 * const char *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        ApplicationLaunchedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
    {
        ApplicationTerminatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_hidden"))
    {
        ApplicationHiddenHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_unhidden"))
    {
        ApplicationUnhiddenHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_created"))
    {
        WindowCreatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_destroyed"))
    {
        WindowDestroyedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        UpdateWindowCollection();
        return true;
    }

    return false;
}

internal bool
Init()
{
    DisplayList = AXLibDisplayList(&DisplayCount);
    Assert(DisplayCount != 0);
    MainDisplay = DisplayList[0];

    uint32_t ProcessPolicy = Process_Policy_Regular | Process_Policy_LSUIElement;
    std::vector<macos_application *> Applications = AXLibRunningProcesses(ProcessPolicy);

    for(size_t Index = 0; Index < Applications.size(); ++Index)
    {
        macos_application *Application = Applications[Index];
        AddApplication(Application);
        AddApplicationWindowList(Application);
    }

    /* NOTE(koekeishiya): Tile windows visible on the current space using binary space partitioning. */
    // CreateWindowTree(MainDisplay);

    bool Result = true;
    return Result;
}

internal void
Deinit()
{
}

/*
 * NOTE(koekeishiya):
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
    return Init();
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
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

    chunkwm_export_window_created,
    chunkwm_export_window_destroyed,

    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Tiling", "0.0.1")
