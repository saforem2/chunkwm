#ifndef CGL_WINDOW_H
#define CGL_WINDOW_H

#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <stdint.h>

typedef int CGSConnectionID;
typedef uint32_t CGSWindowID;
typedef int CGWindowLevel;
typedef int CGSSurfaceID;
typedef CFTypeRef CGSRegionRef;

struct cgl_window;
#define CGL_WINDOW_EVENT_CALLBACK(name) void name(struct cgl_window *window, EventRef event, void *user_data)
typedef CGL_WINDOW_EVENT_CALLBACK(cgl_window_event_callback);

enum cgl_window_event_modifier
{
    CGL_EVENT_MOD_CMD   = cmdKey,
    CGL_EVENT_MOD_SHIFT = shiftKey,
    CGL_EVENT_MOD_CAPS  = alphaLock,
    CGL_EVENT_MOD_ALT   = optionKey,
    CGL_EVENT_MOD_CTRL  = controlKey,
    CGL_EVENT_MOD_NUM   = kEventKeyModifierNumLockMask,
    CGL_EVENT_MOD_FN    = kEventKeyModifierFnMask
};

enum cgl_window_gl_profile
{
    CGL_WINDOW_GL_LEGACY = 0,
    CGL_WINDOW_GL_CORE   = 1
};

struct cgl_window
{
    CGSConnectionID connection;
    CGSWindowID id;
    ProcessSerialNumber psn;
    CGLContextObj context;
    CGSSurfaceID surface;
    CGWindowLevel level;
    CGFloat x, y, width, height;
    GLint v_sync;
    enum cgl_window_gl_profile gl_profile;
    cgl_window_event_callback *mouse_callback;
    cgl_window_event_callback *key_callback;
    cgl_window_event_callback *application_callback;
};

void cgl_window_set_application_callback(struct cgl_window *window, cgl_window_event_callback *mouse_callback);
void cgl_window_set_mouse_callback(struct cgl_window *window, cgl_window_event_callback *mouse_callback);
void cgl_window_set_key_callback(struct cgl_window *window, cgl_window_event_callback *key_callback);

void cgl_window_poll_events(struct cgl_window *window, void *user_data);
void cgl_window_bring_to_front(struct cgl_window *window);

int cgl_window_init(struct cgl_window *window, CGFloat x, CGFloat y, CGFloat width, CGFloat height, int level, enum cgl_window_gl_profile gl_profile, GLint v_sync);
void cgl_window_destroy(struct cgl_window *window);

int cgl_window_move(struct cgl_window *window, float x, float y);
int cgl_window_resize(struct cgl_window *window, float x, float y, float width, float height);
void cgl_window_set_alpha(struct cgl_window *window, float alpha);
void cgl_window_set_sticky(struct cgl_window *window, bool sticky);

void cgl_window_make_current(struct cgl_window *window);
CGLError cgl_window_flush(struct cgl_window *window);

#endif
