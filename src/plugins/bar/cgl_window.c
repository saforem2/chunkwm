#include "cgl_window.h"

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
CGError CGSSetWindowShape(CGSConnectionID cid, CGWindowID wid, float x_offset, float y_offset, const CGSRegionRef shape);
OSStatus CGSOrderWindow(CGSConnectionID cid, CGSWindowID wid, enum CGSWindowOrderingMode place, CGSWindowID relativeToWindow /* nullable */);
CGError CGSMoveWindow(CGSConnectionID cid, CGSWindowID wid, CGPoint *window_pos);
CGError CGSSetWindowOpacity(CGSConnectionID cid, CGSWindowID wid, bool isOpaque);
CGError CGSSetWindowAlpha(CGSConnectionID cid, CGSWindowID wid, float alpha);
CGError CGSSetWindowLevel(CGSConnectionID cid, CGSWindowID wid, CGWindowLevel level);
CGError CGSAddSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID *sid);
CGError CGSRemoveSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
CGError CGSSetSurfaceBounds(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, CGRect rect);
CGError CGSOrderSurface(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, int a, int b);
CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
CGContextRef CGWindowContextCreate(CGSConnectionID cid, CGSWindowID wid, CFDictionaryRef options);
CGError CGSSetWindowTags(CGSConnectionID cid, CGSWindowID wid, const int tags[2], size_t tag_size);
CGError CGSClearWindowTags(CGSConnectionID cid, CGSWindowID wid, const int tags[2], size_t tag_size);
#ifdef __cplusplus
}
#endif

static int cgl_gl_profiles[2] = { kCGLOGLPVersion_Legacy, kCGLOGLPVersion_3_2_Core };

static int
cgl_window_surface_init(struct cgl_window *window)
{
    GLint surface_opacity = 0;
    CGLSetParameter(window->context, kCGLCPSurfaceOpacity, &surface_opacity);

    if (CGSAddSurface(window->connection, window->id, &window->surface) != kCGErrorSuccess) {
        goto err;
    }

    if (CGSSetSurfaceBounds(window->connection, window->id, window->surface, CGRectMake(0, 0, window->width, window->height)) != kCGErrorSuccess) {
        goto err_surface;
    }

    if (CGSOrderSurface(window->connection, window->id, window->surface, 1, 0) != kCGErrorSuccess) {
        goto err_surface;
    }

    if (CGLSetSurface(window->context, window->connection, window->id, window->surface) != kCGLNoError) {
        goto err_surface;
    }

    return 1;

err_surface:
    CGSRemoveSurface(window->connection, window->id, window->surface);

err:
    return 0;
}

static int
cgl_window_context_init(struct cgl_window *window)
{
    CGLPixelFormatObj pixel_format;
    GLint drawable;
    GLint num;

    CGLPixelFormatAttribute attributes[] = {
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute) cgl_gl_profiles[window->gl_profile],
        (CGLPixelFormatAttribute) 0
    };

    CGLChoosePixelFormat(attributes, &pixel_format, &num);
    if(!pixel_format) {
        goto err;
    }

    CGLCreateContext(pixel_format, NULL, &window->context);
    if(!window->context) {
        goto err_pix_fmt;
    }

    CGLSetParameter(window->context, kCGLCPSwapInterval, &window->v_sync);

    if (!cgl_window_surface_init(window)) {
        goto err_context;
    }

    CGLGetParameter(window->context, kCGLCPHasDrawable, &drawable);
    if(!drawable) {
        goto err_context;
    }

    CGLDestroyPixelFormat(pixel_format);
    return 1;

err_context:
    CGLDestroyContext(window->context);

err_pix_fmt:
    CGLDestroyPixelFormat(pixel_format);

err:
    return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
int cgl_window_init(struct cgl_window *window, CGFloat x, CGFloat y, CGFloat width, CGFloat height, int level, enum cgl_window_gl_profile gl_profile, GLint v_sync)
{
    int result = 0;
    CGContextRef context;
    CGSRegionRef region;
    CGRect rect;

    window->connection = CGSMainConnectionID();
    if(!window->connection) {
        goto err;
    }

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->level = level;
    window->gl_profile = gl_profile;
    window->v_sync = v_sync;
    GetCurrentProcess(&window->psn);

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

    // TransformProcessType(&window->psn, kProcessTransformToForegroundApplication);
    cgl_window_bring_to_front(window);

    context = CGWindowContextCreate(window->connection, window->id, 0);
    CGContextClearRect(context, rect);
    CGContextRelease(context);

    result = cgl_window_context_init(window);

err_region:
    CFRelease(region);

err:
    return result;
}
#pragma clang diagnostic pop

int cgl_window_move(struct cgl_window *window, float x, float y)
{
   CGPoint window_pos = { .x = x, .y = y };

   if (CGSMoveWindow(window->connection, window->id, &window_pos) != kCGErrorSuccess) {
       return 0;
   }

   window->x = x;
   window->y = y;
   return 1;
}

int cgl_window_resize(struct cgl_window *window, float x, float y, float width, float height)
{
    int result = 0;
    CGSRegionRef shape;
    CGRect rect = CGRectMake(0, 0, width, height);

    if (CGSNewRegionWithRect(&rect, &shape) != kCGErrorSuccess) {
        goto err;
    }

    if (CGSSetWindowShape(window->connection, window->id, x, y, shape) != kCGErrorSuccess) {
        goto err_region;
    }

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;

    if (window->surface) {
        CGSRemoveSurface(window->connection, window->id, window->surface);
    }
    cgl_window_surface_init(window);
    glViewport(0, 0, width, height);
    result = 1;

err_region:
    CFRelease(shape);

err:
    return result;
}

void cgl_window_set_alpha(struct cgl_window *window, float alpha)
{
    CGSSetWindowAlpha(window->connection, window->id, alpha);
}

void cgl_window_set_sticky(struct cgl_window *window, bool sticky)
{
    int tags[2] = {0};
    tags[0] |= (1 << 11);
    if (sticky) {
        CGSSetWindowTags(window->connection, window->id, tags, 32);
    } else {
        CGSClearWindowTags(window->connection, window->id, tags, 32);
    }
}


void cgl_window_destroy(struct cgl_window *window)
{
    CGLDestroyContext(window->context);
    CGSReleaseWindow(window->connection, window->id);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
void cgl_window_bring_to_front(struct cgl_window *window)
{
    CGSOrderWindow(window->connection, window->id, kCGSOrderAbove, 0);
    SetFrontProcess(&window->psn);
}
#pragma clang diagnostic pop

void cgl_window_set_mouse_callback(struct cgl_window *window, cgl_window_event_callback *mouse_callback)
{
    window->mouse_callback = mouse_callback;
}

void cgl_window_set_key_callback(struct cgl_window *window, cgl_window_event_callback *key_callback)
{
    window->key_callback = key_callback;
}

void cgl_window_set_application_callback(struct cgl_window *window, cgl_window_event_callback *application_callback)
{
    window->application_callback = application_callback;
}

void cgl_window_make_current(struct cgl_window *window)
{
    CGLSetCurrentContext(window->context);
}

CGLError cgl_window_flush(struct cgl_window *window)
{
    return CGLFlushDrawable(window->context);
}

#if 0
static void
debug_print_event_class(OSType event_class)
{
    switch (event_class) {
    case kEventClassMouse:          { printf("kEventClassMouse:%d\t", event_class);         } break;
    case kEventClassKeyboard:       { printf("kEventClassKeyboard:%d\t", event_class);      } break;
    case kEventClassTextInput:      { printf("kEventClassTextInput:%d\t", event_class);     } break;
    case kEventClassApplication:    { printf("kEventClassApplication:%d\t", event_class);   } break;
    case kEventClassAppleEvent:     { printf("kEventClassAppleEvent:%d\t", event_class);    } break;
    case kEventClassMenu:           { printf("kEventClassMenu:%d\t", event_class);          } break;
    case kEventClassWindow:         { printf("kEventClassWindow:%d\t", event_class);        } break;
    case kEventClassControl:        { printf("kEventClassControl:%d\t", event_class);       } break;
    case kEventClassCommand:        { printf("kEventClassCommand:%d\t", event_class);       } break;
    case kEventClassTablet:         { printf("kEventClassTablet:%d\t", event_class);        } break;
    case kEventClassVolume:         { printf("kEventClassVolume:%d\t", event_class);        } break;
    case kEventClassAppearance:     { printf("kEventClassAppearance:%d\t", event_class);    } break;
    case kEventClassService:        { printf("kEventClassService:%d\t", event_class);       } break;
    case kEventClassToolbar:        { printf("kEventClassToolbar:%d\t", event_class);       } break;
    case kEventClassToolbarItem:    { printf("kEventClassToolbarItem:%d\t", event_class);   } break;
    case kEventClassAccessibility:  { printf("kEventClassAccessibility:%d\t", event_class); } break;
    default:                        { printf("event class unknown:%d\t", event_class);      } break;
    }
}
#endif

void cgl_window_poll_events(struct cgl_window *window, void *user_data)
{
    EventTargetRef event_target = GetEventDispatcherTarget();
    EventRef event;

    while (ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &event) == noErr) {
        OSType event_class = GetEventClass(event);

        if (event_class == kEventClassMouse) {
            if (window->mouse_callback) {
                window->mouse_callback(window, event, user_data);
            }
        } else if (event_class == kEventClassKeyboard) {
            if (window->key_callback) {
                window->key_callback(window, event, user_data);
            }
        } else if (event_class == kEventClassApplication) {
            if (window->application_callback) {
                window->application_callback(window, event, user_data);
            }
        }

        SendEventToEventTarget(event, event_target);
        ReleaseEvent(event);
    }
}
