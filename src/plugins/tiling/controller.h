#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

void FocusWindow(char *Direction);
void SwapWindow(char *Direction);
void WarpWindow(char *Direction);
void ToggleWindow(char *Type);
void UseInsertionPoint(char *Direction);
void TemporaryRatio(char *Ratio);
void AdjustWindowRatio(char *Direction);

void RotateWindowTree(char *Degrees);
void MirrorWindowTree(char *Direction);

void ActivateSpaceLayout(char *Layout);
void ToggleSpace(char *Op);

void SendWindowToDesktop(char *Op);
void SendWindowToMonitor(char *Op);

struct macos_window;
void FloatWindow(macos_window *Window);
void UnfloatWindow(macos_window *Window);

void FocusMonitor(char *Op);

#endif
