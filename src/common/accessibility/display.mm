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
extern "C" CFArrayRef CGSCopyWindowsWithOptionsAndTags(CGSConnectionID Connection, unsigned Owner, CFArrayRef Spaces, unsigned Options, unsigned long long *SetTags, unsigned long long *ClearTags);
extern "C" CGError CGSGetWindowLevel(CGSConnectionID Connection, uint32_t WindowId, uint32_t *WindowLevel);


extern "C" CGSSpaceID CGSManagedDisplayGetCurrentSpace(CGSConnectionID Connection, CFStringRef DisplayRef);
extern "C" CFStringRef CGSCopyManagedDisplayForSpace(const CGSConnectionID Connection, CGSSpaceID SpaceId);
extern "C" CFStringRef CGSCopyManagedDisplayForWindow(const CGSConnectionID Connection, uint32_t WindowId);
extern "C" bool CGSManagedDisplayIsAnimating(const CGSConnectionID Connection, CFStringRef DisplayRef);

extern "C" void CGSGetMenuBarAutohideEnabled(CGSConnectionID Connection, int *Status);;
extern "C" Boolean CoreDockGetAutoHideEnabled(void);
extern "C" void  CoreDockGetOrientationAndPinning(macos_dock_orientation *Orientation, int *Pinning);
extern "C" void CoreDockGetRect(CGRect *rect);


extern "C" CFUUIDRef CGDisplayCreateUUIDFromDisplayID(uint32_t DisplayID);

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

    CGRect Frame = CGDisplayBounds(CGDisplayList[0]);
    CGDirectDisplayID BestResult = 0;

    for (unsigned Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID DisplayId = CGDisplayList[Index];
        CGRect DisplayFrame = CGDisplayBounds(DisplayId);

        if (Frame.origin.y + Frame.size.height <= DisplayFrame.origin.y) {
            continue;
        }

        if (DisplayFrame.origin.y + DisplayFrame.size.height <= Frame.origin.y) {
            continue;
        }

        if (Side == -1) {
            if (CGRectGetMidX(DisplayFrame) < CGRectGetMidX(Frame)) {
                Frame = DisplayFrame;
                BestResult = DisplayId;
            }
        } else if (Side == 1) {
            if (CGRectGetMidX(DisplayFrame) > CGRectGetMidX(Frame)) {
                Frame = DisplayFrame;
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

    CGRect Frame = CGDisplayBounds(CGDisplayList[0]);
    CGDirectDisplayID BestResult = 0;

    for (unsigned Index = 0; Index < Count; ++Index) {
        CGDirectDisplayID DisplayId = CGDisplayList[Index];
        CGRect DisplayFrame = CGDisplayBounds(DisplayId);

        if (Frame.origin.x + Frame.size.width <= DisplayFrame.origin.x) {
            continue;
        }

        if (DisplayFrame.origin.x + DisplayFrame.size.width <= Frame.origin.x) {
            continue;
        }

        if (CGRectGetMidY(DisplayFrame) > CGRectGetMidY(Frame)) {
            Frame = DisplayFrame;
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

/*
 * NOTE(koekeishiya): Construct a macos_space representing the active space for the
 * display that currently holds the given macos_window.
 * Caller is responsible for calling 'AXLibDestroySpace()'.
 */
macos_space *AXLibActiveSpace(AXUIElementRef WindowRef, uint32_t WindowId)
{
    __AppleGetDisplayIdentifierFromWindow(WindowRef, WindowId);
    ASSERT(DisplayRef);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    __AppleFreeDisplayIdentifierFromWindow();
    return Space;
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
bool AXLibCGSSpaceIDToDesktopID(CGSSpaceID SpaceId, unsigned *OutArrangement, unsigned *OutDesktopId, bool IncludeFullscreenSpaces)
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
                if (IncludeFullscreenSpaces || UserCreatedSpace) {
                    DesktopId = SpaceIndex;
                } else {
                    DesktopId = 0;
                }
                Result = true;
                goto End;
            }

            if (IncludeFullscreenSpaces || UserCreatedSpace) {
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

bool AXLibCGSSpaceIDToDesktopID(CGSSpaceID SpaceId, unsigned *OutArrangement, unsigned *OutDesktopId)
{
    return AXLibCGSSpaceIDToDesktopID(SpaceId, OutArrangement, OutDesktopId, false);
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
bool AXLibCGSSpaceIDFromDesktopID(unsigned DesktopId, unsigned *OutArrangement, CGSSpaceID *OutSpaceId, bool IncludeFullscreenSpaces)
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

            if (!IncludeFullscreenSpaces) {
                if (CurrentSpaceType != kCGSSpaceUser) {
                    continue;
                }
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

bool AXLibCGSSpaceIDFromDesktopID(unsigned DesktopId, unsigned *OutArrangement, CGSSpaceID *OutSpaceId)
{
    return AXLibCGSSpaceIDFromDesktopID(DesktopId, OutArrangement, OutSpaceId, false);
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

internal bool
IsWindowLevelAllowed(int WindowLevel)
{
    static int ValidWindowLevels[] = {
        CGWindowLevelForKey(kCGBaseWindowLevelKey),
        CGWindowLevelForKey(kCGMinimumWindowLevelKey),
        CGWindowLevelForKey(kCGNormalWindowLevelKey),
        CGWindowLevelForKey(kCGFloatingWindowLevelKey),
        CGWindowLevelForKey(kCGModalPanelWindowLevelKey),
        CGWindowLevelForKey(kCGDraggingWindowLevelKey),
        CGWindowLevelForKey(kCGScreenSaverWindowLevelKey),
        CGWindowLevelForKey(kCGMaximumWindowLevelKey),
        CGWindowLevelForKey(kCGOverlayWindowLevelKey),
        CGWindowLevelForKey(kCGHelpWindowLevelKey),
        CGWindowLevelForKey(kCGUtilityWindowLevelKey),
        CGWindowLevelForKey(kCGAssistiveTechHighWindowLevelKey),
    };
    static int Count = sizeof(ValidWindowLevels) / sizeof(*ValidWindowLevels);

    for (int Index = 0; Index < Count; ++Index) {
        if (WindowLevel == ValidWindowLevels[Index]) {
            return true;
        }
    }

    return false;
}

int *AXLibSpaceWindows(CGSSpaceID SpaceId, int *Count, bool FilterWindowLevels)
{
    int NumberOfWindows;
    NSArray *NSArraySpace = @[ @(SpaceId) ];
    unsigned long long SetTags = 0;
    unsigned long long ClearTags = 0;
    int *Result = NULL;
    int Counter = 0;

    CFArrayRef Windows = CGSCopyWindowsWithOptionsAndTags(CGSDefaultConnection, 0, (__bridge CFArrayRef) NSArraySpace, 1 << 1, &SetTags, &ClearTags);
    if (!Windows) {
        goto out;
    }

    NumberOfWindows = CFArrayGetCount(Windows);
    Result = (int *) malloc(sizeof(int *) * NumberOfWindows);

    for (int Index = 0; Index < NumberOfWindows; ++Index) {
        NSNumber *Id = (__bridge NSNumber *) CFArrayGetValueAtIndex(Windows, Index);
        int WindowId = [Id intValue];
        if (FilterWindowLevels) {
            int WindowLevel = -1;
            CGSGetWindowLevel(CGSDefaultConnection, (uint32_t)WindowId, (uint32_t*)&WindowLevel);
            if (IsWindowLevelAllowed(WindowLevel)) {
                Result[Counter++] = WindowId;
            }
        } else {
            Result[Counter++] = WindowId;
        }
    }

    CFRelease(Windows);

    if (!Counter) {
        free(Result);
        Result = NULL;
    }

out:
    *Count = Counter;
    return Result;
}

int *AXLibSpaceWindows(CGSSpaceID SpaceId, int *Count)
{
    return AXLibSpaceWindows(SpaceId, Count, false);
}

int *AXLibAllWindows(int *Count)
{
    NSMutableArray *SpaceIds = [[NSMutableArray alloc] init];

    CFArrayRef DisplayDictionaries = CGSCopyManagedDisplaySpaces(CGSDefaultConnection);
    for (NSDictionary *DisplayDictionary in (__bridge NSArray *) DisplayDictionaries) {
        NSArray *SpaceDictionaries = DisplayDictionary[@"Spaces"];
        for (NSDictionary *SpaceDictionary in SpaceDictionaries) {
            CGSSpaceID SpaceId = [SpaceDictionary[@"id64"] intValue];
            [SpaceIds addObject:@(SpaceId)];
        }
    }

    unsigned long long SetTags = 0;
    unsigned long long ClearTags = 0;

    CFArrayRef Windows = CGSCopyWindowsWithOptionsAndTags(CGSDefaultConnection, 0, (__bridge CFArrayRef) SpaceIds, 1 << 1, &SetTags, &ClearTags);
    if (!Windows) {
        *Count = 0;
        return NULL;
    }

    int NumberOfWindows = CFArrayGetCount(Windows);
    int *Result = (int *) malloc(sizeof(int) * NumberOfWindows);

    for (int Index = 0; Index < NumberOfWindows; ++Index) {
        NSNumber *Id = (__bridge NSNumber *) CFArrayGetValueAtIndex(Windows, Index);
        Result[Index] = [Id intValue];
    }

    *Count = NumberOfWindows;
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

CGRect AXLibGetDockRect()
{
    CGRect Rect = {};
    CoreDockGetRect(&Rect);
    return Rect;
}

bool AXLibDisplayHasSeparateSpaces()
{
    return [NSScreen screensHaveSeparateSpaces];
}
