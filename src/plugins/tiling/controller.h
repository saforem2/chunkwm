#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

#include <stdint.h>

struct macos_window;
struct macos_space;
struct virtual_space;

bool FindClosestWindow(macos_space *Space, virtual_space *VirtualSpace,
                       macos_window *Match, macos_window **ClosestWindow,
                       char *Direction, bool Wrap);
void CenterMouseInWindow(macos_window *Window);

void FocusWindow(char *Direction);
void SwapWindow(char *Direction);
void WarpWindow(char *Direction);
void WarpFloatingWindow(char *Op);
void ToggleWindow(char *Type);
void UseInsertionPoint(char *Direction);
void TemporaryRatio(char *Ratio);
void AdjustWindowRatio(char *Direction);

void RotateWindowTree(char *Degrees);
void MirrorWindowTree(char *Direction);
void EqualizeWindowTree(char *Unused);

void ActivateSpaceLayout(char *Layout);
void ToggleSpace(char *Op);

void AdjustSpacePadding(char *Op);
void AdjustSpaceGap(char *Op);

bool SendWindowToDesktop(macos_window *Window, char *Op);
void SendWindowToDesktop(char *Op);
void SendWindowToMonitor(char *Op);

void FloatWindow(macos_window *Window, bool UserInitiated);

void FocusMonitor(char *Op);

void SerializeDesktop(char *Op);
void DeserializeDesktop(char *Op);

char *QueryWindowDetails(uint32_t WindowId);
char *QueryWindowsForActiveSpace();

#endif
