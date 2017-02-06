#ifndef CHUNKWM_CORE_STATE_H
#define CHUNKWM_CORE_STATE_H

#include <Carbon/Carbon.h>

struct macos_window;
bool AddWindowToCollection(macos_window *Window);
void RemoveWindowFromCollection(macos_window *Window);
void UpdateWindowCollection();

void UpdateWindowTitle(macos_window *Window);

struct macos_application;
macos_application *GetApplicationFromPID(pid_t PID);
macos_application *ConstructAndAddApplication(ProcessSerialNumber PSN, pid_t PID, char *ProcessName);
void RemoveAndDestroyApplication(macos_application *Application);

bool InitState();

#endif
