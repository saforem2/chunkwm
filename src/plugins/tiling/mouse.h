#ifndef PLUGIN_MOUSE_H
#define PLUGIN_MOUSE_H

EVENTTAP_CALLBACK(EventTapCallback);
bool BindMouseMoveAction(const char *BindSym);
bool BindMouseResizeAction(const char *BindSym);

#endif
