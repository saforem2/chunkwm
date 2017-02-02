#include "display.h"
#include <Cocoa/Cocoa.h>

#define internal static
#define CGSDefaultConnection _CGSDefaultConnection()

typedef int CGSConnectionID;

extern "C" CGSConnectionID _CGSDefaultConnection(void);
extern "C" CGSSpaceType CGSSpaceGetType(CGSConnectionID Connection, CGSSpaceID Id);
extern "C" CFArrayRef CGSCopyManagedDisplaySpaces(const CGSConnectionID Connection);

/* NOTE(koekeishiya): Find the UUID for a given CGDirectDisplayID. */
internal CFStringRef
AXLibDisplayIdentifier(CGDirectDisplayID Id)
{
    CFUUIDRef UUIDRef = CGDisplayCreateUUIDFromDisplayID(Id);
    if(UUIDRef)
    {
        CFStringRef Ref = CFUUIDCreateString(NULL, UUIDRef);
        CFRelease(UUIDRef);
        return Ref;
    }

    return NULL;
}

macos_display *AXLibConstructDisplay(CGDirectDisplayID Id, unsigned Arrangement)
{
    macos_display *Display = (macos_display *) malloc(sizeof(macos_display));

    Display->Ref = AXLibDisplayIdentifier(Id);
    Display->Id = Id;
    Display->Arrangement = Arrangement;

    CGRect Frame = CGDisplayBounds(Id);

    Display->Region.X = Frame.origin.x;
    Display->Region.Y = Frame.origin.y;

    Display->Region.Width = Frame.size.width;
    Display->Region.Height = Frame.size.height;

    return Display;
}

void AXLibDestroyDisplay(macos_display *Display)
{
    CFRelease(Display->Ref);
    free(Display);
}

#define MAX_DISPLAY_COUNT 10
macos_display **AXLibDisplayList(unsigned *Count)
{
    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    *Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, Count);

    macos_display **DisplayList =
        (macos_display **) malloc(*Count * sizeof(macos_display *));

    for(size_t Index = 0;
        Index < *Count;
        ++Index)
    {
        CGDirectDisplayID Id = CGDisplayList[Index];
        DisplayList[Index] = AXLibConstructDisplay(Id, Index);
    }

    free(CGDisplayList);
    return DisplayList;
}

internal macos_space *
AXLibConstructSpace(CFStringRef Ref, CGSSpaceID Id, CGSSpaceType Type)
{
    macos_space *Space = (macos_space *) malloc(sizeof(macos_space));

    Space->Ref = Ref;
    Space->Id = Id;
    Space->Type = Type;

    return Space;
}

internal CFStringRef
AXLibActiveSpaceIdentifier(CFStringRef DisplayRef)
{
    CFStringRef ActiveSpace = NULL;
    NSString *CurrentIdentifier = (__bridge NSString *)DisplayRef;

    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for(NSDictionary *DisplayDictionary in (__bridge NSArray *)DisplayDictionaries)
    {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if([DisplayIdentifier isEqualToString:CurrentIdentifier])
        {
            ActiveSpace = (__bridge CFStringRef) [[NSString alloc] initWithString:DisplayDictionary[@"Current Space"][@"uuid"]];
            break;
        }
    }

    CFRelease(DisplayDictionaries);
    return ActiveSpace;
}

internal CGSSpaceID
AXLibActiveSpaceID(CFStringRef DisplayRef)
{
    CGSSpaceID ActiveSpace = 0;
    NSString *CurrentIdentifier = (__bridge NSString *)DisplayRef;

    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for(NSDictionary *DisplayDictionary in (__bridge NSArray *)DisplayDictionaries)
    {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if([DisplayIdentifier isEqualToString:CurrentIdentifier])
        {
            ActiveSpace = [DisplayDictionary[@"Current Space"][@"id64"] intValue];
            break;
        }
    }

    CFRelease(DisplayDictionaries);
    return ActiveSpace;
}

macos_space *AXLibActiveSpace(CFStringRef DisplayRef)
{
    CGSSpaceID SpaceId = AXLibActiveSpaceID(DisplayRef);
    CFStringRef SpaceRef = AXLibActiveSpaceIdentifier(DisplayRef);
    CGSSpaceType SpaceType = CGSSpaceGetType(CGSDefaultConnection, SpaceId);

    macos_space *Space = AXLibConstructSpace(SpaceRef, SpaceId, SpaceType);
    return Space;
}

void AXLibDestroySpace(macos_space *Space)
{
    CFRelease(Space->Ref);
    free(Space);
}

// CGDisplayRegisterReconfigurationCallback(AXDisplayReconfigurationCallBack, NULL);
