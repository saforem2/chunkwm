#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <pthread.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"

typedef int CGSConnectionID;
typedef uint32_t CGSWindowID;
typedef int CGSSurfaceID;
typedef void *CGSRegion;
typedef int CGSValue;

enum CGSWindowOrderingMode
{
   kCGSOrderAbove = 1,
   kCGSOrderBelow = -1,
   kCGSOrderOut = 0
};

extern "C"
{
    CGSConnectionID CGSMainConnectionID(void);
    CGError CGSNewWindow(CGSConnectionID cid, int, float, float, const CGSRegion, CGSWindowID *);
    CGError CGSReleaseWindow(CGSConnectionID cid, CGWindowID wid);
    CGError CGSNewRegionWithRect(const CGRect * rect, CGSRegion *newRegion);
    OSStatus CGSOrderWindow(CGSConnectionID cid, CGSWindowID wid, CGSWindowOrderingMode place, CGSWindowID relativeToWindow /* nullable */);
    CGError CGSAddSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID *sid);
    CGError CGSSetSurfaceBounds(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, CGRect rect);
    CGError CGSOrderSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, int a, int b);
    CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
}

#define internal static
#define kCGSBufferedBackingType 2

internal const char *PluginName = "bar";
internal const char *PluginVersion = "0.0.1";
internal chunkwm_api API;

internal CGSConnectionID Connection;
internal CGLContextObj GlContext;
internal CGSWindowID Window;

internal pthread_t BarThread;
internal bool Quit;

bool CreateWindow(int X, int Y, int Width, int Height)
{
    Connection = CGSMainConnectionID();
    if(!Connection) return false;
    assert(Connection);

    CGSRegion Region;
    CGRect Rect = CGRectMake(0,0, Width, Height);
    CGSNewRegionWithRect(&Rect, &Region);
    if(!Region) return false;
    CGSNewWindow(Connection, kCGSBufferedBackingType, X, Y, Region, &Window);
    if(!Window) return false;
    OSStatus Error = CGSOrderWindow(Connection, Window, kCGSOrderAbove, 0);
    if(Error != kCGErrorSuccess) return false;
    return true;
}

bool CreateCGLContext(int Width, int Height)
{
    assert(Connection);
    assert(Window);

    CGLPixelFormatAttribute Attributes[] = {
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        (CGLPixelFormatAttribute)0
    };

    GLint Num;
    CGLPixelFormatObj PixelFormat;
    CGLError GlError = CGLChoosePixelFormat(Attributes, &PixelFormat, &Num);
    if(GlError != kCGLNoError) return false;
    if(!PixelFormat) return false;

    CGLCreateContext(PixelFormat, NULL, &GlContext);
    assert(GlContext);
    CGLDestroyPixelFormat(PixelFormat);
    CGLSetCurrentContext(GlContext);

    GLint VSyncEnabled = 1;
    CGLSetParameter(GlContext, kCGLCPSwapInterval, &VSyncEnabled);

    CGSSurfaceID Surface;
    CGError Error = CGSAddSurface(Connection, Window, &Surface);
    if(Error != kCGErrorSuccess) return false;
    Error = CGSSetSurfaceBounds(Connection, Window, Surface, CGRectMake(0, 0, Width, Height));
    if(Error != kCGErrorSuccess) return false;
    Error = CGSOrderSurface(Connection, Window, Surface, 1, 0);
    if(Error != kCGErrorSuccess) return false;

    GlError = CGLSetSurface(GlContext, Connection, Window, Surface);
    if(GlError != kCGLNoError) return false;

    GLint Drawable;
    CGLGetParameter(GlContext, kCGLCPHasDrawable, &Drawable);
    if(!Drawable) return false;

    CGLSetCurrentContext(NULL);
    return true;
}

void *BarMainThreadProcedure(void*)
{
    CGLSetCurrentContext(GlContext);

    while(!Quit)
    {
        glClearColor(0,1,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        static float a = 0;
        glRotatef(a * 1000, 0, 0, 1);
        a= a + .001;
        glBegin(GL_QUADS);
        if (a>1.5) a=0;
        glColor4f(0,a,1,1);
        glVertex2f(0.25, 0.25);
        glVertex2f(0.75, 0.25);
        glVertex2f(0.75, 0.75);
        glVertex2f(0.25, 0.75);
        glEnd();

        CGLError GlError = CGLFlushDrawable(GlContext);
        assert(GlError == kCGLNoError);
        assert(glGetError() == GL_NO_ERROR);
    }

    CGLSetCurrentContext(NULL);
    CGLDestroyContext(GlContext);
    return NULL;
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    return false;
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;
    if(!CreateWindow(100, 100, 500, 500))
    {
        goto out;
    }

    if(!CreateCGLContext(500, 500))
    {
        goto release_window;
    }

    if(glGetError() != GL_NO_ERROR)
    {
        goto release_cgl_context;
    }

    pthread_create(&BarThread, NULL, &BarMainThreadProcedure, NULL);
    return true;

release_cgl_context:
    CGLDestroyContext(GlContext);

release_window:
    CGSReleaseWindow(Connection, Window);
out:
    return false;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    Quit = true;
    pthread_join(BarThread, NULL);
    CGSReleaseWindow(Connection, Window);
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] = { };
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN(PluginName, PluginVersion);
