#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include "constants.h"

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

internal unsigned DisplayCount;
internal macos_display **DisplayList;
internal macos_display *MainDisplay;

internal macos_application_map Applications;
internal macos_window_map Windows;

internal uint32_t InsertionPointId;

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

internal bool
IsWindowValid(macos_window *Window)
{
    bool Result = ((AXLibIsWindowStandard(Window)) &&
                   (AXLibHasFlags(Window, Window_Movable)) &&
                   (AXLibHasFlags(Window, Window_Resizable)));
    return Result;
}

internal void
ExtendedDockSetTopmost(macos_window *Window)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[64];
        sprintf(Message, "window_level %d %d", Window->Id, kCGFloatingWindowLevelKey);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

internal void
FloatWindow(macos_window *Window)
{
    AXLibAddFlags(Window, Window_Float);
    if(CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST))
    {
        ExtendedDockSetTopmost(Window);
    }
}

internal void
TileWindow(macos_display *Display, macos_window *Window)
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

    macos_space *Space = AXLibActiveSpace(Display->Ref);
    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Display, Space);
        if(VirtualSpace->Mode != Virtual_Space_Float)
        {
            if(VirtualSpace->Tree)
            {
                node *Exists = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
                if(!Exists)
                {
                    node *Node = NULL;
                    if(InsertionPointId)
                    {
                        Node = GetNodeWithId(VirtualSpace->Tree, InsertionPointId, VirtualSpace->Mode);
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

                        CreateLeafNodePair(Display, Node, Node->WindowId, Window->Id, Split);
                        ApplyNodeRegion(Node, VirtualSpace->Mode);
                    }
                    else if(VirtualSpace->Mode == Virtual_Space_Monocle)
                    {
                        if(!Node)
                        {
                            Node = GetLastLeafNode(VirtualSpace->Tree);
                            ASSERT(Node != NULL);
                        }

                        node *NewNode = CreateRootNode(Display);
                        NewNode->WindowId = Window->Id;

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
                VirtualSpace->Tree = CreateRootNode(Display);
                VirtualSpace->Tree->WindowId = Window->Id;
                ResizeWindowToRegionSize(VirtualSpace->Tree);
            }
        }
    }

    AXLibDestroySpace(Space);
}

internal void
UntileWindow(macos_display *Display, macos_window *Window)
{
    if(AXLibHasFlags(Window, Window_Float))
    {
        return;
    }

    if(!IsWindowValid(Window))
    {
        return;
    }

    macos_space *Space = AXLibActiveSpace(Display->Ref);
    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Display, Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            node *Node = GetNodeWithId(VirtualSpace->Tree, Window->Id, VirtualSpace->Mode);
            if(Node)
            {
                if(VirtualSpace->Mode == Virtual_Space_Bsp)
                {
                    node *Parent = Node->Parent;
                    if(Parent && Parent->Left && Parent->Right)
                    {
                        node *Child = IsRightChild(Node) ? Parent->Left : Parent->Right;
                        Parent->Left = NULL;
                        Parent->Right = NULL;

                        Parent->WindowId = Child->WindowId;
                        if(Child->Left && Child->Right)
                        {
                            Parent->Left = Child->Left;
                            Parent->Left->Parent = Parent;

                            Parent->Right = Child->Right;
                            Parent->Right->Parent = Parent;

                            CreateNodeRegionRecursive(Display, Parent, true);
                        }

                        ApplyNodeRegion(Parent, VirtualSpace->Mode);
                        free(Child);
                        free(Node);
                    }
                    else if(!Parent)
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

    AXLibDestroySpace(Space);
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
GetAllWindowsToAddToTree(macos_space *Space, std::vector<uint32_t> &VisibleWindows, std::vector<uint32_t> &WindowsInTree)
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

        if((!Found) &&
           (AXLibSpaceHasWindow(Space->Id, WindowId)) &&
           (!AXLibStickyWindow(WindowId)))
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
        for(size_t WindowIndex = 0;
            WindowIndex < VisibleWindows.size();
            ++WindowIndex)
        {
            if(VisibleWindows[WindowIndex] == WindowsInTree[Index])
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

internal void
CreateWindowTree(macos_display *Display)
{
    macos_space *Space = AXLibActiveSpace(Display->Ref);
    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Display, Space);
        if(!VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            std::vector<uint32_t> Windows = GetAllVisibleWindows();
            if(!Windows.empty())
            {
                node *Root = CreateRootNode(Display);
                Root->WindowId = Windows[0];
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

                        CreateLeafNodePair(Display, Node, Node->WindowId, Windows[Index], Split);
                    }
                }
                else if(VirtualSpace->Mode == Virtual_Space_Monocle)
                {
                    for(size_t Index = 1;
                        Index < Windows.size();
                        ++Index)
                    {
                        node *Next = CreateRootNode(Display);
                        Next->WindowId = Windows[Index];

                        Root->Right = Next;
                        Next->Left = Root;
                        Root = Next;
                    }
                }

                ApplyNodeRegion(VirtualSpace->Tree, VirtualSpace->Mode);
            }
        }
    }

    AXLibDestroySpace(Space);
}

internal void
RebalanceWindowTree(macos_display *Display)
{
    macos_space *Space = AXLibActiveSpace(Display->Ref);
    if(Space->Type == kCGSSpaceUser)
    {
        virtual_space *VirtualSpace = AcquireVirtualSpace(Display, Space);
        if(VirtualSpace->Tree && VirtualSpace->Mode != Virtual_Space_Float)
        {
            std::vector<uint32_t> Windows = GetAllVisibleWindows();
            std::vector<uint32_t> WindowsInTree = GetAllWindowsInTree(VirtualSpace->Tree, VirtualSpace->Mode);
            std::vector<uint32_t> WindowsToAdd = GetAllWindowsToAddToTree(Space, Windows, WindowsInTree);
            std::vector<uint32_t> WindowsToRemove = GetAllWindowsToRemoveFromTree(Windows, WindowsInTree);

            for(size_t Index = 0;
                Index < WindowsToRemove.size();
                ++Index)
            {
                macos_window *Window = GetWindowByID(WindowsToRemove[Index]);
                if(Window)
                {
                    UntileWindow(Display, Window);
                }
            }

            for(size_t Index = 0;
                Index < WindowsToAdd.size();
                ++Index)
            {
                macos_window *Window = GetWindowByID(WindowsToAdd[Index]);
                if(Window)
                {
                    TileWindow(Display, Window);
                }
            }
        }
    }

    AXLibDestroySpace(Space);
}

void ApplicationLaunchedHandler(const char *Data)
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
                TileWindow(MainDisplay, Window);
            }
        }

        free(WindowList);
    }
}

void ApplicationTerminatedHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;

    RemoveApplication(Application);
    RebalanceWindowTree(MainDisplay);
}

void ApplicationHiddenHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    RebalanceWindowTree(MainDisplay);
}

void ApplicationUnhiddenHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_space *Space = AXLibActiveSpace(MainDisplay->Ref);
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
                        TileWindow(MainDisplay, Window);
                    }
                }

                AXLibDestroyWindow(Window);
            }

            free(WindowList);
        }
    }

    AXLibDestroySpace(Space);
}

void ApplicationActivatedHandler(const char *Data)
{
    macos_application *Application = (macos_application *) Data;
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        CFRelease(WindowRef);

        macos_window *Window = GetWindowByID(WindowId);
        if((Window) &&
           (IsWindowValid(Window)) &&
           (!AXLibHasFlags(Window, Window_Float)))
        {
            InsertionPointId = Window->Id;
        }
    }
    else
    {
        InsertionPointId = 0;
    }
}

void WindowCreatedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);

    TileWindow(MainDisplay, Copy);
}

void WindowDestroyedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = RemoveWindowFromCollection(Window);
    ASSERT(Copy);

    UntileWindow(MainDisplay, Copy);
    AXLibDestroyWindow(Copy);
}

void WindowMinimizedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    ASSERT(Copy);

    UntileWindow(MainDisplay, Copy);
}

void WindowDeminimizedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_space *Space = AXLibActiveSpace(MainDisplay->Ref);
    if((Space->Type == kCGSSpaceUser) &&
       (AXLibSpaceHasWindow(Space->Id, Window->Id)))
    {
        macos_window *Copy = GetWindowByID(Window->Id);
        ASSERT(Copy);

        TileWindow(MainDisplay, Copy);
    }

    AXLibDestroySpace(Space);
}

void WindowFocusedHandler(const char *Data)
{
    macos_window *Window = (macos_window *) Data;

    macos_window *Copy = GetWindowByID(Window->Id);
    ASSERT(Copy);

    if(IsWindowValid(Copy) && !AXLibHasFlags(Copy, Window_Float))
    {
        InsertionPointId = Copy->Id;
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
    else if(StringsAreEqual(Node, "chunkwm_export_application_activated"))
    {
        ApplicationActivatedHandler(Data);
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
    else if(StringsAreEqual(Node, "chunkwm_export_window_minimized"))
    {
        WindowMinimizedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_deminimized"))
    {
        WindowDeminimizedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_focused"))
    {
        WindowFocusedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        UpdateWindowCollection();
        CreateWindowTree(MainDisplay);
        RebalanceWindowTree(MainDisplay);
        return true;
    }

    return false;
}

internal bool
Init()
{
    BeginCVars();

    CreateCVar(CVAR_SPACE_MODE, Virtual_Space_Bsp);

    CreateCVar(CVAR_SPACE_OFFSET_TOP, 60.0f);
    CreateCVar(CVAR_SPACE_OFFSET_BOTTOM, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_LEFT, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_RIGHT, 50.0f);
    CreateCVar(CVAR_SPACE_OFFSET_GAP, 20.0f);

    CreateCVar(CVAR_BSP_SPAWN_LEFT, 1);
    CreateCVar(CVAR_BSP_OPTIMAL_RATIO, 1.618f);
    CreateCVar(CVAR_BSP_SPLIT_RATIO, 0.5f);
    CreateCVar(CVAR_BSP_SPLIT_MODE, Split_Optimal);

    CreateCVar(CVAR_WINDOW_FLOAT_NEXT, 0);

    /* NOTE(koekeishiya): The following cvars do nothing for now. */

    CreateCVar("mouse_follows_focus", 1);
    CreateCVar("window_float_center", 0);

    /*   ---------------------------------------------------------   */

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

    DisplayList = AXLibDisplayList(&DisplayCount);
    ASSERT(DisplayCount != 0);
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
    CreateWindowTree(MainDisplay);

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

            if(IsWindowValid(Window) && !AXLibHasFlags(Window, Window_Float))
            {
                InsertionPointId = Window->Id;
            }
        }
    }

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

    for(unsigned Index = 0;
        Index < DisplayCount;
        ++Index)
    {
        macos_display *Display = DisplayList[Index];
        AXLibDestroyDisplay(Display);
    }

    ReleaseVirtualSpaces();
    Applications.clear();
    Windows.clear();

    free(DisplayList);
    DisplayList = NULL;
    MainDisplay = NULL;
    DisplayCount = 0;

    EndCVars();
}

/*
 * NOTE(koekeishiya):
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    return Init();
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

    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Tiling", "0.0.1")
