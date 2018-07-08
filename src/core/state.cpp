#include "state.h"

#include "dispatch/event.h"
#include "clog.h"

#include "../common/accessibility/application.h"
#include "../common/accessibility/window.h"
#include "../common/accessibility/element.h"
#include "../common/misc/workspace.h"
#include "../common/misc/assert.h"

#include <pthread.h>

#include <map>
#include <vector>

#define internal static

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

internal macos_application_map Applications;

internal macos_window_map Windows;
internal pthread_mutex_t WindowsLock;

internal inline AXUIElementRef
SystemWideElement()
{
    local_persist AXUIElementRef Element;
    local_persist dispatch_once_t Token;

    dispatch_once(&Token, ^{
        Element = AXUIElementCreateSystemWide();
    });

    return Element;
}

/*
 * NOTE(koekeishiya): We need a way to retrieve AXUIElementRef from a CGWindowID.
 * There is no way to do this, without caching AXUIElementRef references.
 * Here we perform a lookup of macos_window structs.
 */
internal macos_window *
GetWindowByID(uint32_t Id)
{
    pthread_mutex_lock(&WindowsLock);
    macos_window_map_it It = Windows.find(Id);
    macos_window *Result = (It != Windows.end()) ? It->second : NULL;
    pthread_mutex_unlock(&WindowsLock);

    return Result;
}

/*
 * NOTE(koekeishiya): Caller is responsible for making sure that the window is not a dupe.
 * If the window can not be added to the collection, caller is responsible for memory.
 */
bool AddWindowToCollection(macos_window *Window)
{
    bool Result = true;
    AXError Success;

    // NOTE(koekeishiya): A window with id 0 is never valid!
    if (!Window->Id) goto err;

    Success = AXLibAddObserverNotification(&Window->Owner->Observer,
                                           Window->Ref,
                                           kAXUIElementDestroyedNotification,
                                           (void *)(uintptr_t)Window->Id);

    if (Success != kAXErrorSuccess && Success != kAXErrorNotificationAlreadyRegistered) {
        c_log(C_LOG_LEVEL_DEBUG, "%s:%s failed to add kAXUIElementDestroyedNotification '%s'\n", Window->Owner->Name, Window->Name, AXLibAXErrorToString(Success));
        goto err;
    }


    AXLibAddObserverNotification(&Window->Owner->Observer,
                                 Window->Ref,
                                 kAXWindowMiniaturizedNotification,
                                 Window);
    AXLibAddObserverNotification(&Window->Owner->Observer,
                                 Window->Ref,
                                 kAXWindowDeminiaturizedNotification,
                                 Window);
    AXLibAddObserverNotification(&Window->Owner->Observer,
                                 Window->Ref,
                                 kAXSheetCreatedNotification,
                                 Window->Owner);
    AXLibAddObserverNotification(&Window->Owner->Observer,
                                 Window->Ref,
                                 kAXDrawerCreatedNotification,
                                 Window->Owner);

    pthread_mutex_lock(&WindowsLock);
    Windows[Window->Id] = Window;
    pthread_mutex_unlock(&WindowsLock);

    goto out;

err:
    Result = false;

out:
    return Result;
}

// NOTE(koekeishiya): Caller is responsible for passing a valid window!
void RemoveWindowFromCollection(macos_window *Window)
{
    pthread_mutex_lock(&WindowsLock);
    Windows.erase(Window->Id);
    pthread_mutex_unlock(&WindowsLock);

    AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXUIElementDestroyedNotification);
    AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXWindowMiniaturizedNotification);
    AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXWindowDeminiaturizedNotification);
    AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXSheetCreatedNotification);
    AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXDrawerCreatedNotification);
}

// NOTE(koekeishiya): Caller is responsible for passing a valid window!
void UpdateWindowTitle(macos_window *Window)
{
    if (Window->Name) {
        free(Window->Name);
    }

    Window->Name = AXLibGetWindowTitle(Window->Ref);
}

/*
 * NOTE(koekeishiya): Construct macos_windows for an application and add them to our window-collection.
 * If a window is not added to our collection for any reason, we release the memory.
 */
internal void
AddApplicationWindowsToCollection(macos_application *Application)
{
    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if (WindowList) {
        macos_window *Window = NULL;
        macos_window **List = WindowList;

        while ((Window = *List++)) {
            if (GetWindowByID(Window->Id))      goto win_dupe;
            if (!AddWindowToCollection(Window)) goto win_invalid;
            goto success;

win_invalid:
            c_log(C_LOG_LEVEL_DEBUG, "%s:%s is not destructible, ignore!\n", Window->Owner->Name, Window->Name);
            AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXUIElementDestroyedNotification);

win_dupe:
            AXLibDestroyWindow(Window);

success:;
        }

        free(WindowList);
    }
}

// NOTE(koekeishiya): We need a way to retrieve a macos_application * by PID
macos_application *GetApplicationFromPID(pid_t PID)
{
    macos_application *Result = NULL;

    macos_application_map_it It = Applications.find(PID);
    if (It != Applications.end()) {
        Result = It->second;
    }

    return Result;
}

internal void
AddApplication(macos_application *Application)
{
    macos_application_map_it It = Applications.find(Application->PID);
    if (It == Applications.end()) {
        Applications[Application->PID] = Application;
    }
}

internal
OBSERVER_CALLBACK(ApplicationCallback)
{
    macos_application *Application = (macos_application *) Reference;

    if (CFEqual(Notification, kAXWindowCreatedNotification)) {
        macos_window *Window = AXLibConstructWindow(Application, Element);
        ConstructEvent(ChunkWM_WindowCreated, Window);
    } else if (CFEqual(Notification, kAXUIElementDestroyedNotification)) {
        /*
         * NOTE(koekeishiya): If this is an actual window, it should be associated
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
         * At the very least, we need to know the windowid of the destroyed window.
         */

        /* NOTE(koekeishiya): Option 'b' has been implemented. Leave note for future reference. */

        uint32_t WindowId = (uintptr_t) Reference;
        macos_window *Window = GetWindowByID(WindowId);
        if (Window) {
            RemoveWindowFromCollection(Window);
            __sync_or_and_fetch(&Window->Flags, Window_Invalid);
            ConstructEvent(ChunkWM_WindowDestroyed, Window);
        }
    } else if (CFEqual(Notification, kAXFocusedWindowChangedNotification)) {
        /*
         * NOTE(koekeishiya): We have to make sure that we can actually interact with the window.
         * When a window is created, we receive this notification before kAXWindowCreatedNotification.
         * When a window is deminimized, we receive this notification before the window is visible.
         */

        /*
         * NOTE(koekeishiya): To achieve the expected behaviour, we emit a 'ChunkWM_WindowFocused' event
         * after processing 'ChunkWM_WindowCreated' and 'ChunkWM_WindowDeminimized' events.
         */

        uint32_t WindowId = AXLibGetWindowID(Element);
        macos_window *Window = GetWindowByID(WindowId);
        if (Window) {
            ConstructEvent(ChunkWM_WindowFocused, Window);
        }
    } else if (CFEqual(Notification, kAXWindowMovedNotification)) {
        uint32_t WindowId = AXLibGetWindowID(Element);
        macos_window *Window = GetWindowByID(WindowId);
        if (Window) {
            ConstructEvent(ChunkWM_WindowMoved, Window);
        }
    } else if (CFEqual(Notification, kAXWindowResizedNotification)) {
        uint32_t WindowId = AXLibGetWindowID(Element);
        macos_window *Window = GetWindowByID(WindowId);
        if (Window) {
            ConstructEvent(ChunkWM_WindowResized, Window);
        }
    } else if (CFEqual(Notification, kAXWindowMiniaturizedNotification)) {
        /*
         * NOTE(koekeishiya): We cannot register this notification globally for an application.
         * The AXUIElementRef 'Element' we receive cannot be used with 'AXLibGetWindowID', because
         * a window that is minimized often return a CGWindowID of 0. We have to register this
         * notification for every window such that we can pass our own cached window-information.
         */

        macos_window *Window = (macos_window *) Reference;
        ConstructEvent(ChunkWM_WindowMinimized, Window);
    } else if (CFEqual(Notification, kAXWindowDeminiaturizedNotification)) {
        /*
         * NOTE(koekeishiya): when a deminimized window pulls the user to the space of that window,
         * we receive this notification before 'didActiveSpaceChangeNotification'.
         *
         * This does NOT happen if a window is deminimized by cmd-clicking the window. The window
         * will be deminimized on the currently active space, and no space change occur.
         */

        macos_window *Window = (macos_window *) Reference;
        ConstructEvent(ChunkWM_WindowDeminimized, Window);
    } else if ((CFEqual(Notification, kAXSheetCreatedNotification)) ||
               (CFEqual(Notification, kAXDrawerCreatedNotification))) {
        macos_window *Window = AXLibConstructWindow(Application, Element);
        ConstructEvent(ChunkWM_WindowSheetCreated, Window);
    } else if (CFEqual(Notification, kAXTitleChangedNotification)) {
        uint32_t WindowId = AXLibGetWindowID(Element);
        macos_window *Window = GetWindowByID(WindowId);
        if (Window) {
            ConstructEvent(ChunkWM_WindowTitleChanged, Window);
        }
    }
}

#define OBSERVER_RETRIES 20
#define OBSERVER_DELAY 0.1f
#define MICROSEC_PER_SEC 1e6
internal void
ConstructAndAddApplicationDispatch(macos_application *Application, carbon_application_details *Info, float Delay)
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, Delay * NSEC_PER_SEC), dispatch_get_main_queue(),
    ^{
        if (Info->State == Carbon_Application_State_In_Progress) {
            application_launch_state LaunchState = WorkspaceGetApplicationLaunchState(Info->PID);
            if (LaunchState == Application_State_Failed) {
                c_log(C_LOG_LEVEL_DEBUG, "%d:%s could not register window notifications!!!\n", Application->PID, Application->Name);
                Info->State = Carbon_Application_State_Failed;
                AXLibDestroyApplication(Application);
                return;
            } else if (LaunchState == Application_State_Launching) {
                c_log(C_LOG_LEVEL_DEBUG, "%d:%s waiting for application to be ready for notifications\n", Application->PID, Application->Name);
                ConstructAndAddApplicationDispatch(Application, Info, OBSERVER_DELAY);
                return;
            }

            bool Success = AXLibAddApplicationObserver(Application, ApplicationCallback);
            if (Success) {
                c_log(C_LOG_LEVEL_DEBUG, "%d:%s successfully registered window notifications\n", Application->PID, Application->Name);
                Info->State = Carbon_Application_State_Finished;
                AddApplication(Application);
                AddApplicationWindowsToCollection(Application);
                ConstructEvent(ChunkWM_ApplicationLaunched, Info);
            } else {
                if (Application->Observer.Valid) {
                    AXLibDestroyObserver(&Application->Observer);
                }

                if (++Info->Attempts > OBSERVER_RETRIES) {
                    if (Info->State != Carbon_Application_State_Invalid) {
                        Info->State = Carbon_Application_State_Failed;
                    }
                }

                ConstructAndAddApplicationDispatch(Application, Info, OBSERVER_DELAY);
            }
        } else if (Info->State == Carbon_Application_State_Failed) {
            c_log(C_LOG_LEVEL_DEBUG, "%d:%s could not register window notifications!!!\n", Application->PID, Application->Name);
            AXLibDestroyApplication(Application);
        } else if (Info->State == Carbon_Application_State_Invalid) {
            c_log(C_LOG_LEVEL_DEBUG, "%d:%s process terminated; cancel registration of window notifications!!!\n", Application->PID, Application->Name);
            AXLibDestroyApplication(Application);
            ConstructEvent(ChunkWM_ApplicationTerminated, Info);
        }
    });
}

void ConstructAndAddApplication(carbon_application_details *Info)
{
    macos_application *Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);

    Info->State = Carbon_Application_State_In_Progress;
    Info->Attempts = 0;

    ConstructAndAddApplicationDispatch(Application, Info, 0.0f);
}

void RemoveAndDestroyApplication(macos_application *Application)
{
    macos_application_map_it It = Applications.find(Application->PID);
    if (It != Applications.end()) {
        Applications.erase(It);
    }

    AXLibDestroyApplication(Application);
}

void UpdateWindowCollection()
{
    for (macos_application_map_it It = Applications.begin(); It != Applications.end(); ++It) {
        macos_application *Application = It->second;
        AddApplicationWindowsToCollection(Application);
    }
}

// NOTE(koekeishiya): This function is only supposed to be called by our chunkwm main function
bool InitState()
{
    bool Result = pthread_mutex_init(&WindowsLock, NULL) == 0;
    if (Result) {
        NSApplicationLoad();
        AXUIElementSetMessagingTimeout(SystemWideElement(), 1.0);

        uint32_t ProcessPolicy = Process_Policy_Regular | Process_Policy_LSUIElement;
        std::vector<macos_application *> RunningApplications = AXLibRunningProcesses(ProcessPolicy);

        for (size_t Index = 0; Index < RunningApplications.size(); ++Index) {
            macos_application *Application = RunningApplications[Index];
            AddApplication(Application);
            AXLibAddApplicationObserver(Application, ApplicationCallback);
            AddApplicationWindowsToCollection(Application);
        }
    }

    return Result;
}
