#include "element.h"

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
        CFRelease(TypeRef);

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

bool AXLibIsWindowMinimized(AXUIElementRef WindowRef)
{
    bool Result = true;

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
    bool Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXPositionAttribute, (Boolean *)&Result);
    if(Error != kAXErrorSuccess)
        Result = false;

    return Result;
}

bool AXLibIsWindowFullscreen(AXUIElementRef WindowRef)
{
    bool Result = false;

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
    bool Result;

    AXError Error = AXUIElementIsAttributeSettable(WindowRef, kAXSizeAttribute, (Boolean *)&Result);
    if(Error != kAXErrorSuccess)
        Result = false;

    return Result;
}

bool AXLibSetWindowPosition(AXUIElementRef WindowRef, int X, int Y)
{
    bool Result = false;

    CGPoint WindowPos = CGPointMake(X, Y);
    CFTypeRef WindowPosRef = (CFTypeRef)AXValueCreate(kAXValueTypeCGPoint, (const void *)&WindowPos);
    if(WindowPosRef)
    {
        AXError Error = AXLibSetWindowProperty(WindowRef, kAXPositionAttribute, WindowPosRef);
        if(Error == kAXErrorSuccess)
            Result = true;

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
        if(Error == kAXErrorSuccess)
            Result = true;

        CFRelease(WindowSizeRef);
    }

    return Result;
}

/* NOTE(koekeishiya): If a window is minimized when we call this function, the WindowID returned is 0. */
uint32_t AXLibGetWindowID(AXUIElementRef WindowRef)
{
    uint32_t WindowID = kCGNullWindowID;
    _AXUIElementGetWindow(WindowRef, &WindowID);
    return WindowID;
}

char *AXLibGetWindowTitle(AXUIElementRef WindowRef)
{
    CFStringRef WindowTitleRef = (CFStringRef) AXLibGetWindowProperty(WindowRef, kAXTitleAttribute);
    char *WindowTitle = NULL;

    if(WindowTitleRef)
    {
        WindowTitle = CopyCFStringToC(WindowTitleRef, true);
        if(!WindowTitle)
            WindowTitle = CopyCFStringToC(WindowTitleRef, false);

        CFRelease(WindowTitleRef);
    }

    return WindowTitle;
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
