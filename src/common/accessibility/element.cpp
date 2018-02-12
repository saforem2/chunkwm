#include "element.h"
#include "../misc/assert.h"

const char *AXLibAXErrorToString(AXError Error)
{
    switch (Error) {
    case kAXErrorAPIDisabled:                       { return "kAXErrorAPIDisabled";                       } break;
    case kAXErrorActionUnsupported:                 { return "kAXErrorActionUnsupported";                 } break;
    case kAXErrorAttributeUnsupported:              { return "kAXErrorAttributeUnsupported";              } break;
    case kAXErrorCannotComplete:                    { return "kAXErrorCannotComplete";                    } break;
    case kAXErrorFailure:                           { return "kAXErrorFailure";                           } break;
    case kAXErrorIllegalArgument:                   { return "kAXErrorIllegalArgument";                   } break;
    case kAXErrorInvalidUIElement:                  { return "kAXErrorInvalidUIElement";                  } break;
    case kAXErrorInvalidUIElementObserver:          { return "kAXErrorInvalidUIElementObserver";          } break;
    case kAXErrorNoValue:                           { return "kAXErrorNoValue";                           } break;
    case kAXErrorNotEnoughPrecision:                { return "kAXErrorNotEnoughPrecision";                } break;
    case kAXErrorNotImplemented:                    { return "kAXErrorNotImplemented";                    } break;
    case kAXErrorNotificationAlreadyRegistered:     { return "kAXErrorNotificationAlreadyRegistered";     } break;
    case kAXErrorNotificationNotRegistered:         { return "kAXErrorNotificationNotRegistered";         } break;
    case kAXErrorNotificationUnsupported:           { return "kAXErrorNotificationUnsupported";           } break;
    case kAXErrorParameterizedAttributeUnsupported: { return "kAXErrorParameterizedAttributeUnsupported"; } break;
    case kAXErrorSuccess:                           { return "kAXErrorSuccess";                           } break;
    }
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing a valid CFStringRef.
 * Caller is responsible for freeing memory if non-null is returned.
 */
char *CopyCFStringToC(CFStringRef String)
{
    ASSERT(String);
    CFStringEncoding Encoding = kCFStringEncodingUTF8;
    CFIndex Length = CFStringGetLength(String);
    CFIndex Bytes = CFStringGetMaximumSizeForEncoding(Length, Encoding);
    char *Result = (char *) malloc(Bytes + 1);

    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Success = CFStringGetCString(String, Result, Bytes + 1, Encoding);
    if (!Success) {
        free(Result);
        Result = NULL;
    }

    return Result;
}

CGPoint AXLibGetCursorPos()
{
    CGEventRef Event = CGEventCreate(NULL);
    CGPoint Cursor = CGEventGetLocation(Event);
    CFRelease(Event);
    return Cursor;
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing valid arguments.
 * Caller is responsible for calling 'CFRelease()' if non-null is returned.
 */
CFTypeRef AXLibGetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property)
{
    ASSERT(WindowRef);
    ASSERT(Property);
    CFTypeRef TypeRef;
    AXError Error = AXUIElementCopyAttributeValue(WindowRef, Property, &TypeRef);
    bool Result = (Error == kAXErrorSuccess);

    if (!Result && TypeRef) {
        CFRelease(TypeRef);
    }

    return Result ? TypeRef : NULL;
}

/* NOTE(koekeishiya): Caller is responsible for passing valid arguments. */
AXError AXLibSetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property, CFTypeRef Value)
{
    ASSERT(WindowRef);
    ASSERT(Property);
    ASSERT(Value);
    return AXUIElementSetAttributeValue(WindowRef, Property, Value);
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing valid arguments.
 * Caller is responsible for calling 'CFRelease()' if non-null is returned.
 */
AXUIElementRef AXLibGetFocusedWindow(AXUIElementRef ApplicationRef)
{
    ASSERT(ApplicationRef);
    return (AXUIElementRef) AXLibGetWindowProperty(ApplicationRef, kAXFocusedWindowAttribute);
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef.
 * The passed window will become the key-window of its application.
 */
void AXLibSetFocusedWindow(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    AXUIElementSetAttributeValue(WindowRef, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(WindowRef, kAXFocusedAttribute, kCFBooleanTrue);
    // NOTE(koekeishiya): The following raise action is not necessary on Sierra 10.12.6
    // and causes macOS to trigger two focus window events, for the same window reference.
    // AXUIElementPerformAction(WindowRef, kAXRaiseAction);
}

/* NOTE(koekeishiya): Caller is responsible for calling 'CFRelease()'. */
AXUIElementRef AXLibGetFocusedApplication()
{
    pid_t PID;
    ProcessSerialNumber PSN;
    GetFrontProcess(&PSN);
    GetProcessPID(&PSN, &PID);
    return AXUIElementCreateApplication(PID);
}

// NOTE(koekeishiya): The process with the given PSN will become the frontmost application.
void AXLibSetFocusedApplication(ProcessSerialNumber PSN)
{
    SetFrontProcessWithOptions(&PSN, kSetFrontProcessFrontWindowOnly);
}

// NOTE(koekeishiya): The process with the given PID will become the frontmost application.
void AXLibSetFocusedApplication(pid_t PID)
{
    ProcessSerialNumber PSN;
    GetProcessForPID(PID, &PSN);
    AXLibSetFocusedApplication(PSN);
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibIsWindowMinimized(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result = 0;

    CFBooleanRef Value = (CFBooleanRef) AXLibGetWindowProperty(WindowRef, kAXMinimizedAttribute);
    if (Value) {
        Result = CFBooleanGetValue(Value);
        CFRelease(Value);
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibIsWindowMovable(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXPositionAttribute, &Result);
    if (Error != kAXErrorSuccess) {
        Result = 0;
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibIsWindowFullscreen(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result = 0;

    CFBooleanRef Value = (CFBooleanRef) AXLibGetWindowProperty(WindowRef, kAXFullscreenAttribute);
    if (Value) {
        Result = CFBooleanGetValue(Value);
        CFRelease(Value);
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibIsWindowResizable(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXSizeAttribute, &Result);
    if (Error != kAXErrorSuccess) {
        Result = 0;
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibSetWindowPosition(AXUIElementRef WindowRef, float X, float Y)
{
    ASSERT(WindowRef);
    bool Result = false;
    CGPoint WindowPos = CGPointMake(X, Y);

    CFTypeRef WindowPosRef = (CFTypeRef)AXValueCreate(kAXValueTypeCGPoint, (void *)&WindowPos);
    if (WindowPosRef) {
        AXError Error = AXLibSetWindowProperty(WindowRef, kAXPositionAttribute, WindowPosRef);
        Result = (Error == kAXErrorSuccess);
        CFRelease(WindowPosRef);
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
bool AXLibSetWindowSize(AXUIElementRef WindowRef, float Width, float Height)
{
    ASSERT(WindowRef);
    bool Result = false;
    CGSize WindowSize = CGSizeMake(Width, Height);

    CFTypeRef WindowSizeRef = (CFTypeRef)AXValueCreate(kAXValueTypeCGSize, (void *)&WindowSize);
    if (WindowSizeRef) {
        AXError Error = AXLibSetWindowProperty(WindowRef, kAXSizeAttribute, WindowSizeRef);
        Result = (Error == kAXErrorSuccess);
        CFRelease(WindowSizeRef);
    }

    return Result;
}

/* NOTE(koekeishiya): Performs the window close action */
void AXLibCloseWindow(AXUIElementRef WindowRef)
{
    AXUIElementRef Button = NULL;
    if (AXUIElementCopyAttributeValue(WindowRef, kAXCloseButtonAttribute, (CFTypeRef*)&Button) == noErr) {
        AXUIElementPerformAction(Button, kAXPressAction);
        CFRelease(Button);
    }
}

/* NOTE(koekeishiya): Set native fullscreen state. */
bool AXLibSetWindowFullscreen(AXUIElementRef WindowRef, bool Fullscreen)
{
    ASSERT(WindowRef);
    CFBooleanRef Value = Fullscreen ? kCFBooleanTrue : kCFBooleanFalse;
    AXError Error = AXLibSetWindowProperty(WindowRef, kAXFullscreenAttribute, Value);
    bool Result = (Error == kAXErrorSuccess);
    CFRelease(Value);

    return Result;
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef.
 * kCGNullWindowID (0) may be returned if the window is minimized.
 */
uint32_t AXLibGetWindowID(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    uint32_t WindowID = kCGNullWindowID;
    _AXUIElementGetWindow(WindowRef, &WindowID);
    return WindowID;
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef.
 * Caller frees memory.
 */
char *AXLibGetWindowTitle(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    char *Result = NULL;
    CFStringRef WindowTitleRef = (CFStringRef) AXLibGetWindowProperty(WindowRef, kAXTitleAttribute);

    if (WindowTitleRef) {
        char *WindowTitle = CopyCFStringToC(WindowTitleRef);
        if (WindowTitle) {
            Result = WindowTitle;
        }

        CFRelease(WindowTitleRef);
    }

    if (!Result) {
        Result = strdup("<unknown>");
    }

    return Result;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
CGPoint AXLibGetWindowPosition(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    CGPoint WindowPos = {};
    AXValueRef WindowPosRef = (AXValueRef) AXLibGetWindowProperty(WindowRef, kAXPositionAttribute);

    if (WindowPosRef) {
        AXValueGetValue(WindowPosRef, kAXValueTypeCGPoint, &WindowPos);
        CFRelease(WindowPosRef);
    }

    return WindowPos;
}

/* NOTE(koekeishiya): Caller is responsible for passing a valid AXUIElementRef. */
CGSize AXLibGetWindowSize(AXUIElementRef WindowRef)
{
    ASSERT(WindowRef);
    CGSize WindowSize = {};
    AXValueRef WindowSizeRef = (AXValueRef) AXLibGetWindowProperty(WindowRef, kAXSizeAttribute);

    if (WindowSizeRef) {
        AXValueGetValue(WindowSizeRef, kAXValueTypeCGSize, &WindowSize);
        CFRelease(WindowSizeRef);
    }

    return WindowSize;
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing valid arguments.
 * Caller is responsible for calling 'CFRelease()'.
 */
bool AXLibGetWindowRole(AXUIElementRef WindowRef, CFStringRef *Role)
{
    ASSERT(WindowRef);
    ASSERT(Role);
    *Role = (CFStringRef) AXLibGetWindowProperty(WindowRef, kAXRoleAttribute);
    return *Role != NULL;
}

/*
 * NOTE(koekeishiya): Caller is responsible for passing valid arguments.
 * Caller is responsible for calling 'CFRelease()'.
 */
bool AXLibGetWindowSubrole(AXUIElementRef WindowRef, CFStringRef *Subrole)
{
    ASSERT(WindowRef);
    ASSERT(Subrole);
    *Subrole = (CFStringRef) AXLibGetWindowProperty(WindowRef, kAXSubroleAttribute);
    return *Subrole != NULL;
}
