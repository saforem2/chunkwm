#ifndef AXLIB_WINDOW_H
#define AXLIB_WINDOW_H

#include <Carbon/Carbon.h>

enum macos_window_level
{
    // NOTE(koekeishiya): Used by all (?) context menus (firefox, dock, apple applications).
    Window_Level_ContextMenu = 101,

    // NOTE(koekeishiya): Used by Blizzard Battle.net for tooltip windows, probably other applications as well.
    Window_Level_ToolTipWindow = 1000,

    // NOTE(koekeishiya): Used by Blizzard Battle.net for notifications, probably other applications as well.
    Window_Level_Notification = 8,
};

enum macos_window_flags
{
    Window_Init_Minimized = (1 << 0),
    Window_Movable = (1 << 1),
    Window_Resizable = (1 << 2),
    Window_Minimized = (1 << 3),
    Window_Float = (1 << 4),
    Window_Sticky = (1 << 5),
    Window_Invalid = (1 << 6),
    Window_ForceTile = (1 << 7),
};

struct macos_application;
struct macos_window
{
    AXUIElementRef Ref;
    CFStringRef Mainrole;
    CFStringRef Subrole;

    // NOTE(koekeishiya): Store Owner->PID instead ?
    macos_application *Owner;
    uint32_t Id;
    char *Name;

    uint32_t volatile Flags;
    uint32_t Level;

    CGPoint Position;
    CGSize Size;
};

macos_window *AXLibConstructWindow(macos_application *Application, AXUIElementRef WindowRef);
macos_window *AXLibCopyWindow(macos_window *Window);
void AXLibDestroyWindow(macos_window *Window);

bool AXLibIsWindowStandard(macos_window *Window);
bool AXLibWindowHasRole(macos_window *Window, CFTypeRef Role);

macos_window **AXLibWindowListForApplication(macos_application *Application);

inline void
AXLibAddFlags(macos_window *Window, uint32_t Flag)
{
    Window->Flags |= Flag;
}

inline void
AXLibClearFlags(macos_window *Window, uint32_t Flag)
{
    Window->Flags &= ~Flag;
}

inline bool
AXLibHasFlags(macos_window *Window, uint32_t Flag)
{
    bool Result = ((Window->Flags & Flag) != 0);
    return Result;
}

#endif
