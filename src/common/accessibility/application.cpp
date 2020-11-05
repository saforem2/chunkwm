#include "application.h"
#include "../misc/carbon.h"
#include "../misc/workspace.h"
#include "../misc/assert.h"

#define internal static

/*
 * NOTE(koekeishiya): The following files must also be linked against:
 *
 * common/accessibility/observer.cpp
 * common/misc/carbon.cpp
 * common/misc/workspace.mm
 *
 */
internal const char *macos_application_notifications_str[] =
{
    "Application_Notification_WindowCreated",
    "Application_Notification_WindowFocused",
    "Application_Notification_WindowMoved",
    "Application_Notification_WindowResized",
    "Application_Notification_WindowTitleChanged",
    "Application_Notification_Count"
};

enum macos_application_notifications
{
    Application_Notification_WindowCreated,
    Application_Notification_WindowFocused,
    Application_Notification_WindowMoved,
    Application_Notification_WindowResized,
    Application_Notification_WindowTitleChanged,
    Application_Notification_Count
};

internal inline CFStringRef
AXNotificationFromEnum(int Type)
{
    switch(Type) {
    case Application_Notification_WindowCreated: {
        return kAXWindowCreatedNotification;
    } break;
    case Application_Notification_WindowFocused: {
        return kAXFocusedWindowChangedNotification;
    } break;
    case Application_Notification_WindowMoved: {
        return kAXWindowMovedNotification;
    } break;
    case Application_Notification_WindowResized: {
        return kAXWindowResizedNotification;
    } break;
    case Application_Notification_WindowTitleChanged: {
        return kAXTitleChangedNotification;
    } break;
    default: { return NULL; /* NOTE(koekeishiya): Should never happen */ } break;
    }
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid application is passed! */
bool AXLibAddApplicationObserver(macos_application *Application, ObserverCallback Callback)
{
    ASSERT(Application && Application->Ref);

    AXLibConstructObserver(Application, Callback);
    bool Result = Application->Observer.Valid;
    if (Result) {
        for (uint32_t Notification = Application_Notification_WindowCreated;
                      Notification < Application_Notification_Count;
                      ++Notification) {
            AXError Success = AXLibAddObserverNotification(&Application->Observer,
                                                           Application->Ref,
                                                           AXNotificationFromEnum(Notification),
                                                           Application);

            if ((Success != kAXErrorSuccess) && (Success != kAXErrorNotificationAlreadyRegistered)) {
                Result = false;
                break;
            }
        }

        if (Result) {
            AXLibStartObserver(&Application->Observer);
        }
    }

    return Result;
}

/*
 * NOTE(koekeishiya): Passing 'Process_Policy_Regular | Process_Policy_LSUIElement'
 * will filter out any process marked as background-only.
 */
std::vector<macos_application *> AXLibRunningProcesses(uint32_t ProcessFlags)
{
    std::vector<macos_application *> Applications;
    ProcessSerialNumber PSN = { kNoProcess, kNoProcess };
    while (GetNextProcess(&PSN) == noErr) {
        carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);

        bool ValidateProcessPolicy = true;
        bool ValidateProcessBackground = true;

        if (!(ProcessFlags & Process_Policy_Regular)) {
            ValidateProcessPolicy = Info->ProcessPolicy != PROCESS_POLICY_REGULAR;
        }

        if (!(ProcessFlags & Process_Policy_LSUIElement)) {
            ValidateProcessPolicy = Info->ProcessPolicy != PROCESS_POLICY_LSUIELEMENT;
        }

        if (!(ProcessFlags & Process_Policy_LSBackgroundOnly)) {
            ValidateProcessPolicy = Info->ProcessPolicy != PROCESS_POLICY_LSBACKGROUND_ONLY;
        }

        if (!(ProcessFlags & Process_Policy_CarbonBackgroundOnly)) {
            ValidateProcessBackground = Info->ProcessBackground == false;
        }

        if (ValidateProcessPolicy && ValidateProcessBackground) {
            macos_application *Application =
                AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
            Applications.push_back(Application);
        }

        EndCarbonApplicationDetails(Info);
    }

    return Applications;
}

/*
 * NOTE(koekeishiya): Wrap the frontmost application inside a macos_application struct.
 * The caller is responsible for calling 'AXLibDestroyApplication()'.
 */
macos_application *AXLibConstructFocusedApplication()
{
    pid_t PID;
    ProcessSerialNumber PSN;
    GetFrontProcess(&PSN);
    GetProcessPID(&PSN, &PID);
    char *ProcessName = WorkspaceCopyProcessName(PID);

    macos_application *Result = AXLibConstructApplication(PSN, PID, ProcessName);
    free(ProcessName);

    return Result;
}

/* NOTE(koekeishiya): The caller is responsible for calling 'AXLibDestroyApplication()'. */
macos_application *AXLibConstructApplication(ProcessSerialNumber PSN, pid_t PID, char *Name)
{
    macos_application *Application = (macos_application *) malloc(sizeof(macos_application));
    memset(Application, 0, sizeof(macos_application));

    Application->Ref = AXUIElementCreateApplication(PID);
    Application->Name = strdup(Name);
    Application->PSN = PSN;
    Application->PID = PID;

    return Application;
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid application is passed! */
void AXLibDestroyApplication(macos_application *Application)
{
    ASSERT(Application && Application->Ref);

    if (Application->Observer.Valid) {
        AXLibDestroyObserver(&Application->Observer);
    }

    CFRelease(Application->Ref);
    free(Application->Name);
    free(Application);
}
