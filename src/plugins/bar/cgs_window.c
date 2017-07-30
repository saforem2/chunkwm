#include "cgs_window.h"

enum CGSWindowBackingType
{
	kCGSBackingNonRetained = 0,
	kCGSBackingRetained = 1,
	kCGSBackingBuffered = 2
};

enum CGSWindowOrderingMode
{
   kCGSOrderAbove = 1,
   kCGSOrderBelow = -1,
   kCGSOrderOut = 0
};

#ifdef __cplusplus
extern "C" {
#endif
CGSConnectionID CGSMainConnectionID(void);
CGError CGSNewWindow(CGSConnectionID cid, int, float, float, const CGSRegionRef, CGSWindowID *);
CGError CGSReleaseWindow(CGSConnectionID cid, CGWindowID wid);
CGError CGSNewRegionWithRect(const CGRect * rect, CGSRegionRef *newRegion);
OSStatus CGSOrderWindow(CGSConnectionID cid, CGSWindowID wid, CGSWindowOrderingMode place, CGSWindowID relativeToWindow /* nullable */);
CGError CGSSetWindowOpacity(CGSConnectionID cid, CGSWindowID wid, bool isOpaque);
CGError CGSSetWindowLevel(CGSConnectionID cid, CGSWindowID wid, CGWindowLevel level);
CGError CGSAddSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID *sid);
CGError CGSSetSurfaceBounds(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, CGRect rect);
CGError CGSOrderSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, int a, int b);
CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
CGContextRef CGWindowContextCreate(CGSConnectionID cid, CGSWindowID wid, CFDictionaryRef options);
#ifdef __cplusplus
}
#endif

static int
cgs_window_context_init(struct cgs_window *window)
{
    CGLPixelFormatObj pixel_format;
    GLint surface_opacity;
    GLint v_sync_enabled;
    CGLError cgl_error;
    CGError cg_error;
    GLint drawable;
    GLint num;

    CGLPixelFormatAttribute attributes[] = {
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        (CGLPixelFormatAttribute)0
    };

    CGLChoosePixelFormat(attributes, &pixel_format, &num);
    if(!pixel_format) {
        goto err;
    }

    CGLCreateContext(pixel_format, NULL, &window->context);
    if(!window->context) {
        goto err_pix_fmt;
    }

    CGLSetCurrentContext(window->context);

    v_sync_enabled = 1;
    CGLSetParameter(window->context, kCGLCPSwapInterval, &v_sync_enabled);

    surface_opacity = 0;
    CGLSetParameter(window->context, kCGLCPSurfaceOpacity, &surface_opacity);

    cg_error = CGSAddSurface(window->connection, window->id, &window->surface);
    if(cg_error != kCGErrorSuccess) {
        goto err_context;
    }

    cg_error = CGSSetSurfaceBounds(window->connection, window->id, window->surface, CGRectMake(0, 0, window->width, window->height));
    if(cg_error != kCGErrorSuccess) {
        goto err_context;
    }

    cg_error = CGSOrderSurface(window->connection, window->id, window->surface, 1, 0);
    if(cg_error != kCGErrorSuccess) {
        goto err_context;
    }

    cgl_error = CGLSetSurface(window->context, window->connection, window->id, window->surface);
    if(cgl_error != kCGLNoError) {
        goto err_context;
    }

    CGLGetParameter(window->context, kCGLCPHasDrawable, &drawable);
    if(!drawable) {
        goto err_context;
    }

    CGLDestroyPixelFormat(pixel_format);
    CGLSetCurrentContext(NULL);
    return 1;

err_context:
    CGLDestroyContext(window->context);

err_pix_fmt:
    CGLDestroyPixelFormat(pixel_format);

err:
    return 0;
}

int cgs_window_init(struct cgs_window *window)
{
    CGContextRef context;
    CGSRegionRef region;
    CGRect rect;
    int result = 0;

    window->connection = CGSMainConnectionID();
    if(!window->connection) {
        goto err;
    }

    rect = CGRectMake(0, 0, window->width, window->height);
    CGSNewRegionWithRect(&rect, &region);
    if(!region) {
        goto err;
    }

    CGSNewWindow(window->connection, kCGSBackingBuffered, window->x, window->y, region, &window->id);
    if(!window->id) {
        goto err_region;
    }

    CGSSetWindowOpacity(window->connection, window->id, 0);
    CGSSetWindowLevel(window->connection, window->id, CGWindowLevelForKey((CGWindowLevelKey)window->level));
    CGSOrderWindow(window->connection, window->id, kCGSOrderAbove, 0);

    context = CGWindowContextCreate(window->connection, window->id, 0);
    CGContextClearRect(context, rect);
    CGContextRelease(context);

    result = cgs_window_context_init(window);

err_region:
    CFRelease(region);

err:
    return result;
}

void cgs_window_destroy(struct cgs_window *window)
{
    CGLDestroyContext(window->context);
    CGSReleaseWindow(window->connection, window->id);
}
