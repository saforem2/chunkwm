#include "application.h"
#define internal static

enum ax_application_notifications
{
    AXApplication_Notification_WindowCreated,
    AXApplication_Notification_WindowFocused,
    AXApplication_Notification_WindowMoved,
    AXApplication_Notification_WindowResized,
    AXApplication_Notification_WindowTitle,

    AXApplication_Notification_Count
};

internal inline CFStringRef
AXNotificationFromEnum(int Type)
{
    switch(Type)
    {
        case AXApplication_Notification_WindowCreated:
        {
            return kAXWindowCreatedNotification;
        } break;
        case AXApplication_Notification_WindowFocused:
        {
            return kAXFocusedWindowChangedNotification;
        } break;
        case AXApplication_Notification_WindowMoved:
        {
            return kAXWindowMovedNotification;
        } break;
        case AXApplication_Notification_WindowResized:
        {
            return kAXWindowResizedNotification;
        } break;
        case AXApplication_Notification_WindowTitle:
        {
            return kAXTitleChangedNotification;
        } break;
        default: { return NULL; /* NOTE(koekeishiya): Should never happen */ } break;
    }
}

internal inline bool
AXLibHasApplicationObserverNotification(ax_application *Application)
{
    for(int Notification = AXApplication_Notification_WindowCreated;
            Notification < AXApplication_Notification_Count;
            ++Notification)
    {
        if(!(Application->Notifications & (1 << Notification)))
        {
            return false;
        }
    }

    return true;
}

bool AXLibAddApplicationObserver(ax_application *Application, ObserverCallback Callback)
{
    AXLibConstructObserver(Application, Callback);
    if(Application->Observer.Valid)
    {
        for(int Notification = AXApplication_Notification_WindowCreated;
                Notification < AXApplication_Notification_Count;
                ++Notification)
        {
            /* NOTE(koekeishiya): Mark the notification as successful. */
            if(AXLibAddObserverNotification(&Application->Observer, Application->Ref, AXNotificationFromEnum(Notification), Application) == kAXErrorSuccess)
            {
                Application->Notifications |= (1 << Notification);
            }
        }

        AXLibStartObserver(&Application->Observer);
        return AXLibHasApplicationObserverNotification(Application);
    }

    return false;
}

internal void
AXLibRemoveApplicationObserver(ax_application *Application)
{
    if(Application->Observer.Valid)
    {
        AXLibStopObserver(&Application->Observer);

        for(int Notification = AXApplication_Notification_WindowCreated;
                Notification < AXApplication_Notification_Count;
                ++Notification)
        {
            AXLibRemoveObserverNotification(&Application->Observer, Application->Ref, AXNotificationFromEnum(Notification));
        }

        AXLibDestroyObserver(&Application->Observer);
    }
}

ax_application *AXLibConstructApplication(ProcessSerialNumber PSN, pid_t PID, char *Name)
{
    ax_application *Application = (ax_application *) malloc(sizeof(ax_application));
    memset(Application, 0, sizeof(ax_application));

    Application->Ref = AXUIElementCreateApplication(PID);
    Application->Name = strdup(Name);
    Application->PSN = PSN;
    Application->PID = PID;

    return Application;
}

void AXLibDestroyApplication(ax_application *Application)
{
    AXLibRemoveApplicationObserver(Application);
    CFRelease(Application->Ref);
    Application->Ref = NULL;
    free(Application);
}
