#ifndef CGL_CONTEXT_H
#define CGL_CONTEXT_H

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

struct cgs_window
{
    CGSConnectionID connection;
    CGSWindowID id;
    CGLContextObj context;
    CGSSurfaceID surface;
    CGWindowLevel level;
    int x, y, width, height;
};

int cgs_window_init(struct cgs_window *window);
void cgs_window_destroy(struct cgs_window *window);

void cgs_window_make_current(struct cgs_window *window);
CGLError cgs_window_flush(struct cgs_window *window);

#endif
