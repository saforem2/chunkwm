#ifndef CGL_WINDOW_H
#define CGL_WINDOW_H

#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <stdint.h>

#define kCGDesktopWindowLevelKey 2
#define kCGMaximumWindowLevelKey 14

typedef int CGSConnectionID;
typedef uint32_t CGSWindowID;
typedef int CGWindowLevel;
typedef int CGSSurfaceID;
typedef CFTypeRef CGSRegionRef;

struct cgl_window
{
    CGSConnectionID connection;
    CGSWindowID id;
    CGLContextObj context;
    CGSSurfaceID surface;
    CGWindowLevel level;
    int x, y, width, height;
};

int cgl_window_init(struct cgl_window *window, int x, int y, int width, int height, int level);
void cgl_window_destroy(struct cgl_window *window);

void cgl_window_make_current(struct cgl_window *window);
CGLError cgl_window_flush(struct cgl_window *window);

#endif
