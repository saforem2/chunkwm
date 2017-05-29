#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

#include <stdint.h>

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

void SendWindowToDesktop(char *Op);
void SendWindowToMonitor(char *Op);

struct macos_window;
void FloatWindow(macos_window *Window, bool UserInitiated);

void FocusMonitor(char *Op);

void SerializeDesktop(char *Op);
void DeserializeDesktop(char *Op);

char *QueryWindowDetails(uint32_t WindowId);
char *QueryWindowsForActiveSpace();

#endif
