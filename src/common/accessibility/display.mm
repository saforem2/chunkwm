#include "display.h"
#include "../misc/assert.h"

#include <Cocoa/Cocoa.h>

#define internal static
#define CGSDefaultConnection _CGSDefaultConnection()
typedef int CGSConnectionID;

extern "C" CGSConnectionID _CGSDefaultConnection(void);

extern "C" CFDictionaryRef CGSSpaceCopyValues(CGSConnectionID Connection, CGSSpaceID SpaceId);
extern "C" CGSSpaceType CGSSpaceGetType(CGSConnectionID Connection, CGSSpaceID SpaceId);
extern "C" CFArrayRef CGSCopyManagedDisplaySpaces(const CGSConnectionID Connection);
extern "C" CFArrayRef CGSCopySpacesForWindows(CGSConnectionID Connection, CGSSpaceSelector Type, CFArrayRef Windows);
extern "C" void CGSRemoveWindowsFromSpaces(CGSConnectionID Connection, CFArrayRef Windows, CFArrayRef Spaces);
extern "C" void CGSAddWindowsToSpaces(CGSConnectionID Connection, CFArrayRef Windows, CFArrayRef Spaces);
extern "C" void CGSMoveWindowsToManagedSpace(CGSConnectionID Connection, CFArrayRef Windows, CGSSpaceID SpaceId);

extern "C" CGSSpaceID CGSManagedDisplayGetCurrentSpace(CGSConnectionID Connection, CFStringRef DisplayRef);
extern "C" CFStringRef CGSCopyManagedDisplayForSpace(const CGSConnectionID Connection, CGSSpaceID SpaceId);
extern "C" CFStringRef CGSCopyManagedDisplayForWindow(const CGSConnectionID Connection, uint32_t WindowId);
extern "C" bool CGSManagedDisplayIsAnimating(const CGSConnectionID Connection, CFStringRef DisplayRef);

extern "C" void CGSGetMenuBarAutohideEnabled(CGSConnectionID Connection, int *Status);;
extern "C" Boolean CoreDockGetAutoHideEnabled(void);
extern "C" float CoreDockGetTileSize(void);
extern "C" void  CoreDockGetOrientationAndPinning(macos_dock_orientation *Orientation, int *Pinning);

/* NOTE(koekeishiya): Find the UUID associated with a CGDirectDisplayID. */
CFStringRef AXLibGetDisplayIdentifier(CGDirectDisplayID Id)
{
    CFUUIDRef UUIDRef = CGDisplayCreateUUIDFromDisplayID(Id);
    if (UUIDRef) {
        CFStringRef Ref = CFUUIDCreateString(NULL, UUIDRef);
        CFRelease(UUIDRef);
        return Ref;
    }

    return NULL;
}

/* NOTE(koekeishiya): Caller is responsible for calling 'AXLibDestroyDisplay()'. */
macos_display *AXLibConstructDisplay(CGDirectDisplayID Id, unsigned Arrangement)
{
    macos_display *Display = (macos_display *) malloc(sizeof(macos_display));

    Display->Ref = AXLibGetDisplayIdentifier(Id);
    Display->Id = Id;
    Display->Arrangement = Arrangement;

    CGRect Frame = CGDisplayBounds(Id);

    Display->X = Frame.origin.x;
    Display->Y = Frame.origin.y;

    Display->Width = Frame.size.width;
    Display->Height = Frame.size.height;

    return Display;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid display! */
void AXLibDestroyDisplay(macos_display *Display)
{
    ASSERT(Display && Display->Ref);

    CFRelease(Display->Ref);
    free(Display);
}

/* NOTE(koekeishiya): Caller is responsible for all memory (list and entries). */
#define MAX_DISPLAY_COUNT 10
macos_display **AXLibDisplayList(unsigned *Count)
{
    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    *Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, Count);

    macos_display **DisplayList =
        (macos_display **) malloc(*Count * sizeof(macos_display *));

    for (size_t Index = 0; Index < *Count; ++Index) {
        CGDirectDisplayID Id = CGDisplayList[Index];
        DisplayList[Index] = AXLibConstructDisplay(Id, Index);
    }

    free(CGDisplayList);
    return DisplayList;
}

unsigned AXLibDisplayCount()
{
    unsigned Count = 0;

    // NOTE(koekeishiya): We can pass NULL to only get the display count in return
    CGGetActiveDisplayList(0, NULL, &Count);

    return Count;
}

CGRect AXLibGetDisplayBounds(CFStringRef DisplayRef)
{
    CGRect Result = {};

    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    unsigned Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, &Count);

    for (size_t Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID Id = CGDisplayList[Index];
        CFStringRef UUID = AXLibGetDisplayIdentifier(Id);
        if (UUID) {
            if (CFStringCompare(DisplayRef, UUID, 0) == kCFCompareEqualTo) {
                Result = CGDisplayBounds(Id);
                CFRelease(UUID);
                break;
            } else {
                CFRelease(UUID);
            }
        }
    }

    free(CGDisplayList);
    return Result;
}

bool AXLibIsDisplayChangingSpaces(CFStringRef DisplayRef)
{
    return CGSManagedDisplayIsAnimating(CGSDefaultConnection, DisplayRef);
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierFromArrangement(unsigned Arrangement)
{
    unsigned Index = 0;
    CFStringRef Result = NULL;
    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *DisplayDictionary in (__bridge NSArray *) DisplayDictionaries) {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if (Index == Arrangement) {
            Result = (__bridge CFStringRef) [[NSString alloc] initWithString:DisplayIdentifier];
            break;
        }
        ++Index;
    }
    CFRelease(DisplayDictionaries);
    return Result;
}

internal CFStringRef
AXLibGetDisplayIdentifierForEdgeDisplay(int Side)
{
    CFStringRef Result = NULL;

    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    unsigned Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, &Count);

    CGPoint Position = {};
    CGDirectDisplayID BestResult = 0;

    for (unsigned Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID DisplayId = CGDisplayList[Index];
        CGRect DisplayFrame = CGDisplayBounds(DisplayId);

        if (Side == -1) {
            if (DisplayFrame.origin.x < Position.x) {
                Position = DisplayFrame.origin;
                BestResult = DisplayId;
            }
        } else if (Side == 1) {
            if (DisplayFrame.origin.x > Position.x) {
                Position = DisplayFrame.origin;
                BestResult = DisplayId;
            }
        }
    }

    Result = AXLibGetDisplayIdentifier(BestResult);

    free(CGDisplayList);
    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierForBottomMostDisplay()
{
    CFStringRef Result = NULL;

    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    unsigned Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, &Count);

    CGDirectDisplayID BestResult = 0;
    int LargestYCoordinate = 0;

    for (unsigned Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID DisplayId = CGDisplayList[Index];
        CGRect DisplayFrame = CGDisplayBounds(DisplayId);
        int YCoordinate = DisplayFrame.origin.y + DisplayFrame.size.height;

        if (YCoordinate > LargestYCoordinate) {
            LargestYCoordinate = YCoordinate;
            BestResult = DisplayId;
        }
    }

    Result = AXLibGetDisplayIdentifier(BestResult);

    free(CGDisplayList);
    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierForLeftMostDisplay()
{
    return AXLibGetDisplayIdentifierForEdgeDisplay(-1);
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierForRightMostDisplay()
{
    return AXLibGetDisplayIdentifierForEdgeDisplay(1);
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierForMainDisplay()
{
    return AXLibGetDisplayIdentifierFromArrangement(0);
}

/*
 * NOTE(koekeishiya): Caller is responsible for calling CFRelease.
 * This function appears to always return a valid identifier!
 * Could this potentially return NULL if an invalid CGSSpaceID is passed ?
 */
CFStringRef AXLibGetDisplayIdentifierFromSpace(CGSSpaceID SpaceId)
{
    return CGSCopyManagedDisplayForSpace(CGSDefaultConnection, SpaceId);
}

/* NOTE(koekeishiya): Caller is responsible for calling CFRelease. */
CFStringRef AXLibGetDisplayIdentifierFromWindow(uint32_t WindowId)
{
    return CGSCopyManagedDisplayForWindow(CGSDefaultConnection, WindowId);
}

CFStringRef AXLibGetDisplayIdentifierFromWindowRect(CGPoint Position, CGSize Size)
{
    CFStringRef Result = NULL;

    CGDirectDisplayID *CGDisplayList =
        (CGDirectDisplayID *) malloc(sizeof(CGDirectDisplayID) * MAX_DISPLAY_COUNT);

    unsigned Count = 0;
    CGGetActiveDisplayList(MAX_DISPLAY_COUNT, CGDisplayList, &Count);

    CGRect WindowFrame = { Position, Size };
    CGFloat HighestVolume = 0;
    CGDirectDisplayID BestResult = 0;

    for (unsigned Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID DisplayId = CGDisplayList[Index];
        CGRect DisplayFrame = CGDisplayBounds(DisplayId);
        CGRect Intersection = CGRectIntersection(WindowFrame, DisplayFrame);
        CGFloat Volume = Intersection.size.width * Intersection.size.height;

        if (Volume > HighestVolume) {
            HighestVolume = Volume;
            BestResult = DisplayId;
        }
    }

    Result = AXLibGetDisplayIdentifier(BestResult);

    free(CGDisplayList);
    return Result;
}

internal CGSSpaceID
AXLibActiveSpaceIdentifier(CFStringRef DisplayRef, CFStringRef *SpaceRef)
{
    CGSSpaceID ActiveSpaceId = 0;
    NSString *CurrentIdentifier = (__bridge NSString *) DisplayRef;

    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *DisplayDictionary in (__bridge NSArray *) DisplayDictionaries) {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if ([DisplayIdentifier isEqualToString:CurrentIdentifier]) {
            *SpaceRef = (__bridge CFStringRef) [[NSString alloc] initWithString:DisplayDictionary[@"Current Space"][@"uuid"]];
            ActiveSpaceId = [DisplayDictionary[@"Current Space"][@"id64"] intValue];
            break;
        }
    }

    CFRelease(DisplayDictionaries);
    return ActiveSpaceId;
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

CGSSpaceID AXLibActiveCGSSpaceID(CFStringRef DisplayRef)
{
    return CGSManagedDisplayGetCurrentSpace(CGSDefaultConnection, DisplayRef);
}

/*
 * NOTE(koekeishiya): Returns a macos_space representing the active space
 * for the given display. Caller is responsible for calling 'AXLibDestroySpace()'.
 */
macos_space *AXLibActiveSpace(CFStringRef DisplayRef)
{
    ASSERT(DisplayRef);

    CFStringRef SpaceRef;
    CGSSpaceID SpaceId = AXLibActiveSpaceIdentifier(DisplayRef, &SpaceRef);
    CGSSpaceType SpaceType = CGSSpaceGetType(CGSDefaultConnection, SpaceId);

    macos_space *Space = AXLibConstructSpace(SpaceRef, SpaceId, SpaceType);
    return Space;
}

/*
 * NOTE(koekeishiya): Construct a macos_space representing the active space for the
 * display that currently holds the window that accepts key-input.
 * Caller is responsible for calling 'AXLibDestroySpace()'.
 */
bool AXLibActiveSpace(macos_space **Space)
{
    bool Result = false;

    NSDictionary *ScreenDictionary = [[NSScreen mainScreen] deviceDescription];
    NSNumber *ScreenID = [ScreenDictionary objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID DisplayID = [ScreenID unsignedIntValue];

    CFUUIDRef DisplayUUID = CGDisplayCreateUUIDFromDisplayID(DisplayID);
    if (DisplayUUID) {
        CFStringRef Identifier = CFUUIDCreateString(NULL, DisplayUUID);
        *Space = AXLibActiveSpace(Identifier);

        CFRelease(DisplayUUID);
        CFRelease(Identifier);

        Result = true;
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid space! */
void AXLibDestroySpace(macos_space *Space)
{
    ASSERT(Space && Space->Ref);

    CFRelease(Space->Ref);
    free(Space);
}

/*
 * NOTE(koekeishiya): Translate a CGSSpaceID to the index shown in mission control. Also
 * assign the arrangement index of the display that the space belongs to.
 *
 * It is safe to pass NULL for OutArrangement and OutDesktopId in case this information
 * is not of importance to the caller.
 *
 * The function returns a bool specifying if the CGSSpaceID was properly translated.
 */
bool AXLibCGSSpaceIDToDesktopID(CGSSpaceID SpaceId, unsigned *OutArrangement, unsigned *OutDesktopId)
{
    bool Result = false;
    unsigned Arrangement = 0;
    unsigned DesktopId = 0;

    unsigned SpaceIndex = 1;
    CFArrayRef ScreenDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *ScreenDictionary in (__bridge NSArray *) ScreenDictionaries) {
        NSArray *SpaceDictionaries = ScreenDictionary[@"Spaces"];
        for (NSDictionary *SpaceDictionary in (__bridge NSArray *) SpaceDictionaries) {
            CGSSpaceID CurrentSpaceId = [SpaceDictionary[@"id64"] intValue];
            CGSSpaceType CurrentSpaceType = CGSSpaceGetType(CGSDefaultConnection, CurrentSpaceId);
            bool UserCreatedSpace = (CurrentSpaceType == kCGSSpaceUser);

            if (SpaceId == CurrentSpaceId) {
                DesktopId = UserCreatedSpace ? SpaceIndex : 0;
                Result = true;
                goto End;
            }

            if (UserCreatedSpace) {
                ++SpaceIndex;
            }
        }

        ++Arrangement;
    }

End:
    if (OutArrangement) {
        *OutArrangement = Arrangement;
    }

    if (OutDesktopId) {
        *OutDesktopId = DesktopId;
    }

    CFRelease(ScreenDictionaries);
    return Result;
}

/*
 * NOTE(koekeishiya): Translate the space index shown in mission control to a CGSSpaceID.
 * Also assign the arrangement index of the display that the space belongs to.
 *
 * It is safe to pass NULL for OutArrangement and OutSpaceId in case this information
 * is not of importance to the caller.
 *
 * The function returns a bool specifying if the index was properly translated.
 */
bool AXLibCGSSpaceIDFromDesktopID(unsigned DesktopId, unsigned *OutArrangement, CGSSpaceID *OutSpaceId)
{
    bool Result = false;
    CGSSpaceID SpaceId = 0;
    unsigned Arrangement = 0;
    unsigned SpaceIndex = 1;

    CFArrayRef ScreenDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *ScreenDictionary in (__bridge NSArray *) ScreenDictionaries) {
        NSArray *SpaceDictionaries = ScreenDictionary[@"Spaces"];
        for (NSDictionary *SpaceDictionary in (__bridge NSArray *) SpaceDictionaries) {
            CGSSpaceID CurrentSpaceId = [SpaceDictionary[@"id64"] intValue];
            CGSSpaceType CurrentSpaceType = CGSSpaceGetType(CGSDefaultConnection, CurrentSpaceId);

            if (CurrentSpaceType != kCGSSpaceUser) {
                continue;
            }

            if (SpaceIndex == DesktopId) {
                SpaceId = CurrentSpaceId;
                Result = true;
                goto End;
            }

            ++SpaceIndex;
        }

        ++Arrangement;
    }

End:
    if (OutArrangement) {
        *OutArrangement = Arrangement;
    }

    if (OutSpaceId) {
        *OutSpaceId = SpaceId;
    }

    CFRelease(ScreenDictionaries);
    return Result;
}

void AXLibSpaceAddWindow(CGSSpaceID SpaceId, uint32_t WindowId)
{
    NSArray *NSArrayWindow = @[ @(WindowId) ];
    NSArray *NSArrayDestinationSpace = @[ @(SpaceId) ];
    CGSAddWindowsToSpaces(CGSDefaultConnection, (__bridge CFArrayRef)NSArrayWindow, (__bridge CFArrayRef)NSArrayDestinationSpace);
}

void AXLibSpaceRemoveWindow(CGSSpaceID SpaceId, uint32_t WindowId)
{
    NSArray *NSArrayWindow = @[ @(WindowId) ];
    NSArray *NSArraySourceSpace = @[ @(SpaceId) ];
    CGSRemoveWindowsFromSpaces(CGSDefaultConnection, (__bridge CFArrayRef)NSArrayWindow, (__bridge CFArrayRef)NSArraySourceSpace);
}

void AXLibSpaceMoveWindow(CGSSpaceID SpaceId, uint32_t WindowId)
{
    NSArray *NSArrayWindow = @[ @(WindowId) ];
    CGSMoveWindowsToManagedSpace(CGSDefaultConnection, (__bridge CFArrayRef)NSArrayWindow, SpaceId);
}

internal void
AXLibCountSpacesForDisplay(NSDictionary *DisplayDictionary, unsigned *Count)
{
    NSArray *SpaceDictionaries = DisplayDictionary[@"Spaces"];
    for (NSDictionary *SpaceDictionary in SpaceDictionaries) {
        CGSSpaceID CurrentSpaceId = [SpaceDictionary[@"id64"] intValue];
        CGSSpaceType CurrentSpaceType = CGSSpaceGetType(CGSDefaultConnection, CurrentSpaceId);
        if (CurrentSpaceType == kCGSSpaceUser) *Count += 1;
    }
}

int *AXLibSpacesForDisplay(CFStringRef DisplayRef, int *Count)
{
    int *Result = NULL;
    unsigned DesktopIndex = 0;
    unsigned DesktopCount = 0;

    NSString *CurrentIdentifier = (__bridge NSString *) DisplayRef;
    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *DisplayDictionary in (__bridge NSArray *) DisplayDictionaries) {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if ([DisplayIdentifier isEqualToString:CurrentIdentifier]) {
            AXLibCountSpacesForDisplay(DisplayDictionary, &DesktopCount);
            break;
        } else {
            AXLibCountSpacesForDisplay(DisplayDictionary, &DesktopIndex);
        }
    }

    Result = (int *) malloc(sizeof(int *) * DesktopCount);
    *Count = DesktopCount;

    for (int Index = 0; Index < DesktopCount; ++Index) {
        Result[Index] = DesktopIndex + (Index + 1);
    }

    CFRelease(DisplayDictionaries);
    return Result;
}

/*
 * NOTE(koekeishiya): Returns a list of macos_space * structs for all spaces on the given display.
 * The list is terminated by a null-pointer and can be iterated in the following way:
 *
 *     macos_space *Space, **List, **Spaces;
 *     List = Spaces = AXLibSpacesForDisplay(DisplayRef);
 *     while((Space = *List++)) { ..; AXLibDestroySpace(Space); }
 *     free(Spaces);
 */
macos_space **AXLibSpacesForDisplay(CFStringRef DisplayRef)
{
    macos_space **Result = NULL;

    NSString *CurrentIdentifier = (__bridge NSString *) DisplayRef;
    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *DisplayDictionary in (__bridge NSArray *) DisplayDictionaries) {
        NSString *DisplayIdentifier = DisplayDictionary[@"Display Identifier"];
        if ([DisplayIdentifier isEqualToString:CurrentIdentifier]) {
            NSArray *SpaceDictionaries = DisplayDictionary[@"Spaces"];
            int SpaceCount = [SpaceDictionaries count] + 1;
            Result = (macos_space **) malloc(SpaceCount * sizeof(macos_space *));

            int SpaceIndex = 0;
            for (NSDictionary *SpaceDictionary in SpaceDictionaries) {
                CGSSpaceID SpaceId = [SpaceDictionary[@"id64"] intValue];
                CGSSpaceType SpaceType = [SpaceDictionary[@"type"] intValue];
                CFStringRef SpaceRef = (__bridge CFStringRef) [[NSString alloc] initWithString:SpaceDictionary[@"uuid"]];
                macos_space *Space = AXLibConstructSpace(SpaceRef, SpaceId, SpaceType);
                Result[SpaceIndex++] = Space;
            }

            Result[SpaceIndex] = NULL;
        }
    }

    return Result;
}

/*
 * NOTE(koekeishiya): Returns a list of macos_space * structs that show this window.
 * The list is terminated by a null-pointer and can be iterated in the following way:
 *
 *     macos_space *Space, **List, **Spaces;
 *     List = Spaces = AXLibSpacesForWindow(Window->Id);
 *     while((Space = *List++)) { ..; AXLibDestroySpace(Space); }
 *     free(Spaces);
 */
macos_space **
AXLibSpacesForWindow(uint32_t WindowId)
{
    macos_space **Result = NULL;

    NSArray *NSArrayWindow = @[ @(WindowId) ];
    CFArrayRef Spaces = CGSCopySpacesForWindows(CGSDefaultConnection, kCGSSpaceAll, (__bridge CFArrayRef) NSArrayWindow);
    int NumberOfSpaces = CFArrayGetCount(Spaces);

    if (NumberOfSpaces) {
        Result = (macos_space **) malloc(sizeof(macos_space *) * (NumberOfSpaces + 1));

        for (int Index = 0; Index < NumberOfSpaces; ++Index) {
            NSNumber *Id = (__bridge NSNumber *) CFArrayGetValueAtIndex(Spaces, Index);
            CGSSpaceID SpaceId = [Id intValue];

            CFDictionaryRef SpaceCFDictionary = CGSSpaceCopyValues(CGSDefaultConnection, SpaceId);
            NSDictionary *SpaceDictionary = (__bridge NSDictionary *) SpaceCFDictionary;

            CGSSpaceType SpaceType = [SpaceDictionary[@"type"] intValue];
            CFStringRef SpaceRef = (__bridge CFStringRef) [[NSString alloc] initWithString:SpaceDictionary[@"uuid"]];

            macos_space *Space = AXLibConstructSpace(SpaceRef, SpaceId, SpaceType);
            Result[Index] = Space;

            CFRelease(SpaceCFDictionary);
        }

        Result[NumberOfSpaces] = NULL;
        CFRelease(Spaces);
    }

    return Result;
}

bool AXLibSpaceHasWindow(CGSSpaceID SpaceId, uint32_t WindowId)
{
    bool Result = false;

    NSArray *NSArrayWindow = @[ @(WindowId) ];
    CFArrayRef Spaces = CGSCopySpacesForWindows(CGSDefaultConnection, kCGSSpaceAll, (__bridge CFArrayRef) NSArrayWindow);
    int NumberOfSpaces = CFArrayGetCount(Spaces);

    for (int Index = 0; Index < NumberOfSpaces; ++Index) {
        NSNumber *Id = (__bridge NSNumber *) CFArrayGetValueAtIndex(Spaces, Index);
        if (SpaceId == [Id intValue]) {
            Result = true;
            break;
        }
    }

    CFRelease(Spaces);
    return Result;
}

bool AXLibStickyWindow(uint32_t WindowId)
{
    bool Result = false;

    NSArray *NSArrayWindow = @[ @(WindowId) ];
    CFArrayRef Spaces = CGSCopySpacesForWindows(CGSDefaultConnection, kCGSSpaceAll, (__bridge CFArrayRef) NSArrayWindow);
    int NumberOfSpaces = CFArrayGetCount(Spaces);

    Result = NumberOfSpaces > 1;

    CFRelease(Spaces);
    return Result;
}


bool AXLibIsMenuBarAutoHideEnabled()
{
    int Status = 0;
    CGSGetMenuBarAutohideEnabled(CGSDefaultConnection, &Status);
    return Status == 1;
}

bool AXLibIsDockAutoHideEnabled()
{
    return CoreDockGetAutoHideEnabled() != 0;
}

macos_dock_orientation AXLibGetDockOrientation()
{
    int Unused;
    macos_dock_orientation Orientation;
    CoreDockGetOrientationAndPinning(&Orientation, &Unused);
    return Orientation;
}

/*
 * NOTE(koekeishiya): CoreDock API returns a float in the range 0.0 -> 1.0
 * the minimum size of the Dock is 16 and the maximum size is 128. We translate
 * our value back into this number range.
 */
#define DOCK_MAX_TILESIZE 128
#define DOCK_MIN_TILESIZE  16
size_t AXLibGetDockTileSize()
{
    float Ratio = CoreDockGetTileSize();
    size_t Result = (Ratio * (DOCK_MAX_TILESIZE - DOCK_MIN_TILESIZE)) + DOCK_MIN_TILESIZE;
    return Result;
}

bool AXLibDisplayHasSeparateSpaces()
{
    return [NSScreen screensHaveSeparateSpaces];
}
