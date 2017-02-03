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
#include "../../common/dispatch/workspace.h"
#include "../../common/dispatch/cgeventtap.h"
#include "../../common/ipc/daemon.h"

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

internal event_tap EventTap;
internal macos_application_map Applications;
internal pthread_mutex_t ApplicationsMutex;

internal unsigned DisplayCount;
internal macos_display **DisplayList;
internal macos_display *MainDisplay;

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

#include <vector>
#include <queue>

internal std::vector<macos_application *>
RunningProcesses()
{
    std::vector<macos_application *> Applications;
    ProcessSerialNumber PSN = { kNoProcess, kNoProcess };
    while(GetNextProcess(&PSN) == noErr)
    {
        carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);

        /* NOTE(koekeishiya):
         * ProcessPolicy == 0     -> Appears in Dock, default for bundled applications.
         * ProcessPolicy == 1     -> Does not appear in Dock. Can create windows.
         *                           LSUIElement is set to 1.
         * ProcessPolicy == 2     -> Does not appear in Dock, cannot create windows.
         *                           LSBackgroundOnly is set to 1.
         * ProcessBackground == 1 -> Process is a background only process. */

        if((Info->ProcessBackground == 0) &&
           (Info->ProcessPolicy != 2))
        {
            macos_application *Application =
                AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
            Applications.push_back(Application);
        }

        EndCarbonApplicationDetails(Info);
    }

    return Applications;
}

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

macos_application_map *BeginApplications()
{
    pthread_mutex_lock(&ApplicationsMutex);
    return &Applications;
}

void EndApplications()
{
    pthread_mutex_unlock(&ApplicationsMutex);
}

macos_application *GetApplicationFromPID(pid_t PID)
{
    macos_application *Result = NULL;

    BeginApplications();
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Result = It->second;
    }
    EndApplications();

    return Result;
}

internal void
AddApplication(macos_application *Application)
{
    BeginApplications();
    macos_application_map_it It = Applications.find(Application->PID);
    if(It == Applications.end())
    {
        Applications[Application->PID] = Application;
    }
    EndApplications();
}

internal void
RemoveApplication(pid_t PID)
{
    BeginApplications();
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Applications.erase(It);
    }
    EndApplications();
}

/* NOTE(koekeishiya): Iterate through an application window-list (macos_window **)
 * and store them in our collection of valid windows. If we do not add a window,
 * free the allocated memory. */
internal void
AppendApplicationWindowList(macos_window **List)
{
    macos_window *Window = NULL;
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
}

internal macos_window **
ApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = NULL;

    CFArrayRef Windows = (CFArrayRef) AXLibGetWindowProperty(Application->Ref, kAXWindowsAttribute);
    if(Windows)
    {
        CFIndex Count = CFArrayGetCount(Windows);
        WindowList = (macos_window **) malloc((Count + 1) * sizeof(macos_window *));
        WindowList[Count] = NULL;

        for(CFIndex Index = 0; Index < Count; ++Index)
        {
            AXUIElementRef Ref = (AXUIElementRef) CFArrayGetValueAtIndex(Windows, Index);
            macos_window *Window = AXLibConstructWindow(Application, Ref);
            WindowList[Index] = Window;
        }

        CFRelease(Windows);
    }

    return WindowList;
}

internal
OBSERVER_CALLBACK(Callback)
{
    macos_application *Application = (macos_application *) Reference;

    if(CFEqual(Notification, kAXWindowCreatedNotification))
    {
        printf("%s: kAXWindowCreatedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXUIElementDestroyedNotification))
    {
        /* TODO(koekeishiya): If this is an actual window, it should be associated
         * with a valid CGWindowID. HOWEVER, because the window in question has been
         * destroyed. We are unable to utilize this window reference with the AX API.
         *
         * The 'CFEqual()' function can still be used to compare this AXUIElementRef
         * with any existing window refs that we may have. There are a couple of ways
         * we can use to track if an actual window is closed.
         *
         *   a) Store all window AXUIElementRefs in a local cache that we update upon
         *      creation and removal. Requires unsorted container with custom comparator
         *      that uses 'CFEqual()' to match AXUIElementRefs.
         *
         *   b) Instead of tracking 'kAXUIElementDestroyedNotification' for an application,
         *      we have to register this notification separately for every window created.
         *      By doing this, we can pass our own data containing the information necessary
         *      to properly identify and report which window was destroyed.
         *
         * At the very least, we need to know the windowid of the destroyed window. */

        /* NOTE(koekeishiya): Option 'b' seems to be the best choice, as it will allow us to
         * pass a pointer to the macos_window struct, containing all the information we need. */

        printf("%s: kAXUIElementDestroyedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        printf("%s: kAXFocusedWindowChangedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        printf("%s: kAXWindowMiniaturizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        printf("%s: kAXWindowDeminiaturizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        printf("%s: kAXWindowMovedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        printf("%s: kAXWindowResizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXTitleChangedNotification))
    {
        printf("%s: kAXWindowTitleChangedNotification\n", Application->Name);
    }
}

#define MICROSEC_PER_SEC 1e6
void ApplicationLaunchedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    if((Info->ProcessBackground == 0) &&
       (Info->ProcessPolicy != 2))
    {
        macos_application *Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
        if(Application)
        {
            printf("    plugin: launched '%s'\n", Info->ProcessName);
            AddApplication(Application);

            /* NOTE(koekeishiya): We need to wait for some amount of time before we can try to
             * observe the launched application. The time to wait depends on how long the
             * application in question takes to finish. Half a second is good enough for
             * most applications so we 'usleep()' as a temporary fix for now, but we need a way
             * to properly defer the creation of observers for applications that require more time.
             *
             * We cannot simply defer the creation automatically using dispatch_after, because
             * there is simply no way to remove a dispatched event once it has been created.
             * We need a way to tell a dispatched event to NOT execute and be rendered invalid,
             * because some applications only live for a very very short amount of time.
             * The dispatched event will then be triggered after a potential 'terminated' event
             * has been received, in which the application reference has been freed.
             *
             * Passing an invalid reference to the AXObserver API does not simply trigger an error,
             * but causes a full on segmentation fault. */

            usleep(0.5 * MICROSEC_PER_SEC);
            if(AXLibAddApplicationObserver(Application, Callback))
            {
                printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
            }

            // macos_window **WindowList = ApplicationWindowList(Application);
            // free(WindowList);
        }
    }
}

void ApplicationTerminatedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: terminated '%s'\n", Info->ProcessName);
        RemoveApplication(Application->PID);
        AXLibDestroyApplication(Application);
    }
}

void ApplicationHiddenHandler(const char *Data)
{
    workspace_application_details *Info =
        (workspace_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: hidden '%s'\n", Info->ProcessName);
    }
}

void ApplicationUnhiddenHandler(const char *Data)
{
    workspace_application_details *Info =
        (workspace_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: unhidden '%s'\n", Info->ProcessName);
    }
}

internal
DAEMON_CALLBACK(DaemonCallback)
{
    printf("    plugin daemon: %s\n", Message);
}

internal
EVENTTAP_CALLBACK(EventCallback)
{
    event_tap *EventTap = (event_tap *) Reference;

    switch(Type)
    {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
        {
            CGEventTapEnable(EventTap->Handle, true);
        } break;
        case kCGEventMouseMoved:
        {
            printf("kCGEventMouseMoved\n");
        } break;
        case kCGEventLeftMouseDown:
        {
            printf("kCGEventLeftMouseDown\n");
        } break;
        case kCGEventLeftMouseUp:
        {
            printf("kCGEventLeftMouseUp\n");
        } break;
        case kCGEventLeftMouseDragged:
        {
            printf("kCGEventLeftMouseDragged\n");
        } break;
        case kCGEventRightMouseDown:
        {
            printf("kCGEventRightMouseDown\n");
        } break;
        case kCGEventRightMouseUp:
        {
            printf("kCGEventRightMouseUp\n");
        } break;
        case kCGEventRightMouseDragged:
        {
            printf("kCGEventRightMouseDragged\n");
        } break;

        default: {} break;
    }

    return Event;
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
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        printf("Active Space Changed\n");
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

    std::vector<macos_application *> Applications = RunningProcesses();
    for(size_t Index = 0; Index < Applications.size(); ++Index)
    {
        macos_application *Application = Applications[Index];
        macos_window **WindowList = ApplicationWindowList(Application);

        AddApplication(Application);
        AXLibAddApplicationObserver(Application, Callback);

        if(WindowList)
        {
            AppendApplicationWindowList(WindowList);
        }

        free(WindowList);
    }

    CreateWindowTree(MainDisplay);
#if 0
    int Port = 4020;
    EventTap.Mask = ((1 << kCGEventMouseMoved) |
                     (1 << kCGEventLeftMouseDragged) |
                     (1 << kCGEventLeftMouseDown) |
                     (1 << kCGEventLeftMouseUp) |
                     (1 << kCGEventRightMouseDragged) |
                     (1 << kCGEventRightMouseDown) |
                     (1 << kCGEventRightMouseUp));

    bool Result = ((pthread_mutex_init(&ApplicationsMutex, NULL) == 0) &&
                   (StartDaemon(Port, &DaemonCallback)) &&
                   (BeginEventTap(&EventTap, &EventCallback)));
#else
    bool Result = (pthread_mutex_init(&ApplicationsMutex, NULL) == 0);
#endif
    return Result;
}

internal void
Deinit()
{
#if 0
    StopDaemon();
    EndEventTap(&EventTap);
#endif
    pthread_mutex_destroy(&ApplicationsMutex);
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
    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Tiling", "0.0.1")
