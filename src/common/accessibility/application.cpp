#include "application.h"
#include "element.h"

#define internal static

enum macos_application_notifications
{
    Application_Notification_WindowCreated,
    Application_Notification_WindowDestroyed,
    Application_Notification_WindowFocused,
    Application_Notification_WindowMoved,
    Application_Notification_WindowResized,
    Application_Notification_WindowMinimized,
    Application_Notification_WindowDeminimized,
    Application_Notification_WindowTitleChanged,
    Application_Notification_Count
};

internal inline CFStringRef
AXNotificationFromEnum(int Type)
{
    switch(Type)
    {
        case Application_Notification_WindowCreated:
        {
            return kAXWindowCreatedNotification;
        } break;
        case Application_Notification_WindowDestroyed:
        {
            return kAXUIElementDestroyedNotification;
        } break;
        case Application_Notification_WindowFocused:
        {
            return kAXFocusedWindowChangedNotification;
        } break;
        case Application_Notification_WindowMoved:
        {
            return kAXWindowMovedNotification;
        } break;
        case Application_Notification_WindowResized:
        {
            return kAXWindowResizedNotification;
        } break;
        case Application_Notification_WindowMinimized:
        {
            return kAXWindowMiniaturizedNotification;
        } break;
        case Application_Notification_WindowDeminimized:
        {
            return kAXWindowDeminiaturizedNotification;
        } break;
        case Application_Notification_WindowTitleChanged:
        {
            return kAXTitleChangedNotification;
        } break;
        default: { return NULL; /* NOTE(koekeishiya): Should never happen */ } break;
    }
}

/* NOTE(koekeishiya): The caller is responsible for making sure that a valid application is passed! */
bool AXLibAddApplicationObserver(macos_application *Application, ObserverCallback Callback)
{
    AXLibConstructObserver(Application, Callback);
    bool Result = Application->Observer.Valid;
    if(Result)
    {
        for(uint32_t Notification = Application_Notification_WindowCreated;
                     Notification < Application_Notification_Count;
                     ++Notification)
        {
            AXError Success = AXLibAddObserverNotification(&Application->Observer,
                                                           Application->Ref,
                                                           AXNotificationFromEnum(Notification),
                                                           Application);
#if 1
            if(Success == kAXErrorInvalidUIElementObserver)
                printf("OBSERVER ERROR (%s): The observer is not a valid AXObserverRef type.\n", Application->Name);

            if(Success == kAXErrorIllegalArgument)
                printf("OBSERVER ERROR (%s): One or more of the arguments is an illegal value or the length of the notification name is greater than 1024.\n", Application->Name);

            if(Success == kAXErrorNotificationUnsupported)
                printf("OBSERVER ERROR (%s): The accessibility object does not support notifications (note that the system-wide accessibility object does not support notifications).\n", Application->Name);

            if(Success == kAXErrorNotificationAlreadyRegistered)
                printf("OBSERVER ERROR (%s): The notification has already been registered.\n", Application->Name);

            if(Success == kAXErrorCannotComplete)
                printf("OBSERVER ERROR (%s): The function cannot complete because messaging has failed in some way.\n", Application->Name);

            if(Success == kAXErrorFailure)
                printf("OBSERVER ERROR (%s): There is some sort of system memory failure.\n", Application->Name);
#endif
            if(Success != kAXErrorSuccess)
            {
                Result = false;
                break;
            }
        }

        if(Result)
        {
            AXLibStartObserver(&Application->Observer);
        }
    }

    return Result;
}

/* NOTE(koekeishiya): Wrap the frontmost application inside a macos_application struct.
 * The caller is responsible for calling 'AXLibDestroyApplication()'. */
macos_application *AXLibConstructFocusedApplication()
{
    pid_t PID;
    ProcessSerialNumber PSN;
    GetFrontProcess(&PSN);
    GetProcessPID(&PSN, &PID);

    CFStringRef CFProcessName;
    CopyProcessName(&PSN, &CFProcessName);

    // NOTE(koekeishiya): Try UTF-8 encoding first.
    char *ProcessName = CopyCFStringToC(CFProcessName, true);
    if(!ProcessName)
    {
        ProcessName = CopyCFStringToC(CFProcessName, false);
    }
    CFRelease(CFProcessName);

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
    if(Application->Observer.Valid)
    {
        AXLibDestroyObserver(&Application->Observer);
    }

    CFRelease(Application->Ref);
    free(Application->Name);
    free(Application);
}
