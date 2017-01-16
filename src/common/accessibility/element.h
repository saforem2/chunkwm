#ifndef AXLIB_ELEMENT_H
#define AXLIB_ELEMENT_H

#include <Carbon/Carbon.h>

#define kAXFullscreenAttribute CFSTR("AXFullScreen")

extern "C" AXError _AXUIElementGetWindow(AXUIElementRef, uint32_t *WID);
uint32_t AXLibGetWindowID(AXUIElementRef WindowRef);

bool AXLibIsWindowMinimized(AXUIElementRef WindowRef);
bool AXLibIsWindowResizable(AXUIElementRef WindowRef);
bool AXLibIsWindowMovable(AXUIElementRef WindowRef);
bool AXLibIsWindowFullscreen(AXUIElementRef WindowRef);

bool AXLibSetWindowPosition(AXUIElementRef WindowRef, int X, int Y);
bool AXLibSetWindowSize(AXUIElementRef WindowRef, int Width, int Height);

CFTypeRef AXLibGetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property);
AXError AXLibSetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property, CFTypeRef Value);

char *AXLibGetWindowTitle(AXUIElementRef WindowRef);
CGPoint AXLibGetWindowPosition(AXUIElementRef WindowRef);
CGSize AXLibGetWindowSize(AXUIElementRef WindowRef);

bool AXLibGetWindowRole(AXUIElementRef WindowRef, CFTypeRef *Role);
bool AXLibGetWindowSubrole(AXUIElementRef WindowRef, CFTypeRef *Subrole);

CGPoint AXLibGetCursorPos();
char *CopyCFStringToC(CFStringRef String, bool UTF8);

#endif
