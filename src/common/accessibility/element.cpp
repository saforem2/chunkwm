#include "element.h"

// NOTE(koekeishiya): Caller frees memory.
char *CopyCFStringToC(CFStringRef String, bool UTF8)
{
    char *Result = NULL;
    CFStringEncoding Encoding = UTF8 ? kCFStringEncodingUTF8 : kCFStringEncodingMacRoman;

    CFIndex Length = CFStringGetLength(String);
    CFIndex Bytes = CFStringGetMaximumSizeForEncoding(Length, Encoding);

    char *CString = (char *) malloc(Bytes + 1);
    CFStringGetCString(String, CString, Bytes + 1, Encoding);
    if(CString)
    {
        Result = CString;
    }
    else
    {
        free(CString);
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

CFTypeRef AXLibGetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property)
{
    CFTypeRef TypeRef;
    AXError Error = AXUIElementCopyAttributeValue(WindowRef, Property, &TypeRef);
    bool Result = (Error == kAXErrorSuccess);

    if(!Result && TypeRef)
    {
        CFRelease(TypeRef);
    }

    return Result ? TypeRef : NULL;
}

AXError AXLibSetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property, CFTypeRef Value)
{
    return AXUIElementSetAttributeValue(WindowRef, Property, Value);
}

AXUIElementRef AXLibGetFocusedWindow(AXUIElementRef ApplicationRef)
{
    return (AXUIElementRef) AXLibGetWindowProperty(ApplicationRef, kAXFocusedWindowAttribute);
}

// NOTE(koekeishiya): The passed window will become the key-window of its application.
void AXLibSetFocusedWindow(AXUIElementRef WindowRef)
{
    AXUIElementSetAttributeValue(WindowRef, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(WindowRef, kAXFocusedAttribute, kCFBooleanTrue);
    AXUIElementPerformAction(WindowRef, kAXRaiseAction);
}

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

bool AXLibIsWindowMinimized(AXUIElementRef WindowRef)
{
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result = 0;

    CFBooleanRef Value = (CFBooleanRef) AXLibGetWindowProperty(WindowRef, kAXMinimizedAttribute);
    if(Value)
    {
        Result = CFBooleanGetValue(Value);
        CFRelease(Value);
    }

    return Result;
}

bool AXLibIsWindowMovable(AXUIElementRef WindowRef)
{
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXPositionAttribute, &Result);
    if(Error != kAXErrorSuccess)
    {
        Result = 0;
    }

    return Result;
}

bool AXLibIsWindowFullscreen(AXUIElementRef WindowRef)
{
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result = 0;

    CFBooleanRef Value = (CFBooleanRef) AXLibGetWindowProperty(WindowRef, kAXFullscreenAttribute);
    if(Value)
    {
        Result = CFBooleanGetValue(Value);
        CFRelease(Value);
    }

    return Result;
}

bool AXLibIsWindowResizable(AXUIElementRef WindowRef)
{
    // NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
    Boolean Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXSizeAttribute, &Result);
    if(Error != kAXErrorSuccess)
    {
        Result = 0;
    }

    return Result;
}

bool AXLibSetWindowPosition(AXUIElementRef WindowRef, int X, int Y)
{
    bool Result = false;

    CGPoint WindowPos = CGPointMake(X, Y);
    CFTypeRef WindowPosRef = (CFTypeRef)AXValueCreate(kAXValueTypeCGPoint, (void *)&WindowPos);
    if(WindowPosRef)
    {
        AXError Error = AXLibSetWindowProperty(WindowRef, kAXPositionAttribute, WindowPosRef);
        Result = (Error == kAXErrorSuccess);
        CFRelease(WindowPosRef);
    }

    return Result;
}

bool AXLibSetWindowSize(AXUIElementRef WindowRef, int Width, int Height)
{
    bool Result = false;

    CGSize WindowSize = CGSizeMake(Width, Height);
    CFTypeRef WindowSizeRef = (CFTypeRef)AXValueCreate(kAXValueTypeCGSize, (void *)&WindowSize);
    if(WindowSizeRef)
    {
        AXError Error = AXLibSetWindowProperty(WindowRef, kAXSizeAttribute, WindowSizeRef);
        Result = (Error == kAXErrorSuccess);
        CFRelease(WindowSizeRef);
    }

    return Result;
}

/* NOTE(koekeishiya): kCGNullWindowID (0) may be returned if the window is minimized. */
uint32_t AXLibGetWindowID(AXUIElementRef WindowRef)
{
    uint32_t WindowID = kCGNullWindowID;
    _AXUIElementGetWindow(WindowRef, &WindowID);
    return WindowID;
}

// NOTE(koekeishiya): Caller frees memory.
char *AXLibGetWindowTitle(AXUIElementRef WindowRef)
{
    char *Result = "<unknown>";

    CFStringRef WindowTitleRef = (CFStringRef) AXLibGetWindowProperty(WindowRef, kAXTitleAttribute);
    if(WindowTitleRef)
    {
        char *WindowTitle = CopyCFStringToC(WindowTitleRef, true);
        if(!WindowTitle)
        {
            WindowTitle = CopyCFStringToC(WindowTitleRef, false);
        }
        CFRelease(WindowTitleRef);

        if(WindowTitle)
        {
            Result = WindowTitle;
        }
    }

    return Result;
}

CGPoint AXLibGetWindowPosition(AXUIElementRef WindowRef)
{
    CGPoint WindowPos = {};
    AXValueRef WindowPosRef = (AXValueRef) AXLibGetWindowProperty(WindowRef, kAXPositionAttribute);

    if(WindowPosRef)
    {
        AXValueGetValue(WindowPosRef, kAXValueTypeCGPoint, &WindowPos);
        CFRelease(WindowPosRef);
    }

    return WindowPos;
}

CGSize AXLibGetWindowSize(AXUIElementRef WindowRef)
{
    CGSize WindowSize = {};
    AXValueRef WindowSizeRef = (AXValueRef) AXLibGetWindowProperty(WindowRef, kAXSizeAttribute);

    if(WindowSizeRef)
    {
        AXValueGetValue(WindowSizeRef, kAXValueTypeCGSize, &WindowSize);
        CFRelease(WindowSizeRef);
    }

    return WindowSize;
}

bool AXLibGetWindowRole(AXUIElementRef WindowRef, CFTypeRef *Role)
{
    *Role = AXLibGetWindowProperty(WindowRef, kAXRoleAttribute);
    return *Role != NULL;
}

bool AXLibGetWindowSubrole(AXUIElementRef WindowRef, CFTypeRef *Subrole)
{
    *Subrole = AXLibGetWindowProperty(WindowRef, kAXSubroleAttribute);
    return *Subrole != NULL;
}
