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

bool AXLibSetWindowPosition(AXUIElementRef WindowRef, float X, float Y);
bool AXLibSetWindowSize(AXUIElementRef WindowRef, float Width, float Height);
bool AXLibSetWindowFullscreen(AXUIElementRef WindowRef, bool Fullscreen);
void AXLibCloseWindow(AXUIElementRef WindowRef);

CFTypeRef AXLibGetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property);
AXError AXLibSetWindowProperty(AXUIElementRef WindowRef, CFStringRef Property, CFTypeRef Value);

AXUIElementRef AXLibGetFocusedWindow(AXUIElementRef ApplicationRef);
void AXLibSetFocusedWindow(AXUIElementRef WindowRef);

AXUIElementRef AXLibGetFocusedApplication();
void AXLibSetFocusedApplication(ProcessSerialNumber PSN);
void AXLibSetFocusedApplication(pid_t PID);

char *AXLibGetWindowTitle(AXUIElementRef WindowRef);
CGPoint AXLibGetWindowPosition(AXUIElementRef WindowRef);
CGSize AXLibGetWindowSize(AXUIElementRef WindowRef);

bool AXLibGetWindowRole(AXUIElementRef WindowRef, CFStringRef *Role);
bool AXLibGetWindowSubrole(AXUIElementRef WindowRef, CFStringRef *Subrole);

CGPoint AXLibGetCursorPos();
char *CopyCFStringToC(CFStringRef String);

const char *AXLibAXErrorToString(AXError Error);

#endif
