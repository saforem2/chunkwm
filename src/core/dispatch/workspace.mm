#include "workspace.h"
#include "event.h"
#include "../../common/misc/assert.h"

#import <Cocoa/Cocoa.h>

#define internal static

@interface WorkspaceWatcher : NSObject {
}
- (id)init;
@end

internal WorkspaceWatcher *Watcher;
void BeginSharedWorkspace()
{
    Watcher = [[WorkspaceWatcher alloc] init];
}

internal workspace_application_details *
BeginWorkspaceApplicationDetails(NSNotification *Notification)
{
    workspace_application_details *Info =
                    (workspace_application_details *) malloc(sizeof(workspace_application_details));

    const char *Name = [[[Notification.userInfo objectForKey:NSWorkspaceApplicationKey] localizedName] UTF8String];
    Info->PID = [[Notification.userInfo objectForKey:NSWorkspaceApplicationKey] processIdentifier];
    GetProcessForPID(Info->PID, &Info->PSN);
    Info->ProcessName = Name ? strdup(Name) : strdup("<unknown>");

    return Info;
}

// NOTE(koekeishiya): Make sure that the correct module frees memory.
void EndWorkspaceApplicationDetails(workspace_application_details *Info)
{
    ASSERT(Info);

    if(Info->ProcessName)
    {
        free(Info->ProcessName);
    }

    free(Info);
}

/* NOTE(koekeishiya): Subscribe to necessary notifications from NSWorkspace */
@implementation WorkspaceWatcher
- (id)init
{
    if ((self = [super init]))
    {
       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(activeDisplayDidChange:)
                name:@"NSWorkspaceActiveDisplayDidChangeNotification"
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(activeSpaceDidChange:)
                name:NSWorkspaceActiveSpaceDidChangeNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didActivateApplication:)
                name:NSWorkspaceDidActivateApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didDeactivateApplication:)
                name:NSWorkspaceDidDeactivateApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didHideApplication:)
                name:NSWorkspaceDidHideApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didUnhideApplication:)
                name:NSWorkspaceDidUnhideApplicationNotification
                object:nil];
    }

    return self;
}

- (void)dealloc
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
    [super dealloc];
}

- (void)activeDisplayDidChange:(NSNotification *)notification
{
    ConstructEvent(ChunkWM_DisplayChanged, NULL);
}

- (void)activeSpaceDidChange:(NSNotification *)notification
{
    ConstructEvent(ChunkWM_SpaceChanged, NULL);
}

- (void)didActivateApplication:(NSNotification *)notification
{
    workspace_application_details *Info = BeginWorkspaceApplicationDetails(notification);
    ConstructEvent(ChunkWM_ApplicationActivated, Info);
}

- (void)didDeactivateApplication:(NSNotification *)notification
{
    workspace_application_details *Info = BeginWorkspaceApplicationDetails(notification);
    ConstructEvent(ChunkWM_ApplicationDeactivated, Info);
}

- (void)didHideApplication:(NSNotification *)notification
{
    workspace_application_details *Info = BeginWorkspaceApplicationDetails(notification);
    ConstructEvent(ChunkWM_ApplicationHidden, Info);
}

- (void)didUnhideApplication:(NSNotification *)notification
{
    workspace_application_details *Info = BeginWorkspaceApplicationDetails(notification);
    ConstructEvent(ChunkWM_ApplicationVisible, Info);
}

@end
