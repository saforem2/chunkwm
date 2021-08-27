#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

#include <stdint.h>

struct macos_window;
struct macos_space;
struct virtual_space;

void ExtendedDockSetWindowPosition(uint32_t WindowId, int X, int Y);
void ExtendedDockSetWindowAlpha(uint32_t WindowId, float Value, float Duration);
void ExtendedDockSetWindowAlpha(uint32_t WindowId, float Value);
void ExtendedDockSetWindowLevel(macos_window *Window, int WindowLevelKey);
void ExtendedDockSetWindowSticky(macos_window *Window, int Value);

void EnableWindowFading(uint32_t FocusedWindowId);
void DisableWindowFading();

bool FindClosestWindow(macos_space *Space, virtual_space *VirtualSpace,
                       macos_window *Match, macos_window **ClosestWindow,
                       char *Direction, bool Wrap);
void CenterMouseInWindow(macos_window *Window);

void GridLayout(macos_window *Window, char *Op);
void GridLayout(char *Op);
void CloseWindow(char *Unused);
void FocusWindow(char *Direction);
void SwapWindow(char *Direction);
void WarpWindow(char *Direction);
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
bool SendWindowToMonitor(macos_window *Window, char *Op);
void SendWindowToMonitor(char *Op);

void FloatWindow(macos_window *Window);

void FocusMonitor(char *Op);

void SerializeDesktop(char *Op);
void DeserializeDesktop(char *Op);
void FocusDesktop(char *Op);
void CreateDesktop(char *Unused);
void DestroyDesktop(char *Unused);
void MoveDesktop(char *Op);

void QueryWindow(char *Op, int SockFD);
void QueryDesktop(char *Op, int SockFD);
void QueryMonitor(char *Op, int SockFD);
void QueryWindowsForDesktop(char *Op, int SockFD);
void QueryDesktopsForMonitor(char *Op, int SockFD);
void QueryMonitorForDesktop(char *Op, int SockFD);

#endif
