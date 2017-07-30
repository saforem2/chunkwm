#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <pthread.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"

typedef int CGSConnectionID;
typedef uint32_t CGSWindowID;
typedef int CGWindowLevel;
typedef int CGSSurfaceID;
typedef CFTypeRef CGSRegionRef;
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
}

#define internal static
#define kCGSBufferedBackingType 2
#define kCGDesktopWindowLevelKey 2
#define kCGMaximumWindowLevelKey 14

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

    CGSRegionRef Region;
    CGRect Rect = CGRectMake(0,0, Width, Height);
    CGSNewRegionWithRect(&Rect, &Region);
    if(!Region) return false;
    CGSNewWindow(Connection, kCGSBufferedBackingType, X, Y, Region, &Window);
    if(!Window) return false;
    CGSSetWindowOpacity(Connection, Window, 0);
    CGContextRef Context = CGWindowContextCreate(Connection, Window, 0);
    CGContextClearRect(Context, Rect);
    CGContextRelease(Context);
    CGSSetWindowLevel(Connection, Window, CGWindowLevelForKey((CGWindowLevelKey)kCGMaximumWindowLevelKey));
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
        kCGLPFAOpenGLProfile,
        // NOTE(koekeishiya: GL 4.1
        (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        (CGLPixelFormatAttribute)0
    };

    GLint Num;
    CGLPixelFormatObj PixelFormat;
    CGLError GlError = CGLChoosePixelFormat(Attributes, &PixelFormat, &Num);
    if(GlError != kCGLNoError) return false;
    if(!PixelFormat) return false;

    CGLCreateContext(PixelFormat, NULL, &GlContext);
    if(!GlContext) return false;
    CGLDestroyPixelFormat(PixelFormat);
    CGLSetCurrentContext(GlContext);

    GLint VSyncEnabled = 1;
    CGLSetParameter(GlContext, kCGLCPSwapInterval, &VSyncEnabled);
    GLint SurfaceOpacity = 0;
    CGLSetParameter(GlContext, kCGLCPSurfaceOpacity, &SurfaceOpacity);

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

const char *VertexShaderCode =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\n\0";

const char *FragmentShaderCode =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n\0";

void *BarMainThreadProcedure(void*)
{
    CGLSetCurrentContext(GlContext);

    GLint CGLMajor, CGLMinor;
    CGLGetVersion(&CGLMajor, &CGLMinor);
    printf("CGL Version: %d.%d\nOpenGL Version: %s\n",
           CGLMajor, CGLMinor, glGetString(GL_VERSION));

    // NOTE(koekeishiya): wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    GLint Success;
    char InfoLog[512];

    int VertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VertexShader, 1, &VertexShaderCode, NULL);
    glCompileShader(VertexShader);
    glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &Success);
    if(Success == GL_FALSE)
    {
        glGetShaderInfoLog(VertexShader, 512, NULL, InfoLog);
        fprintf(stderr, "ERROR::SHADER::VERTEX::COMPILATION_FAILED: %s\n", InfoLog);
    }

    int FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(FragmentShader, 1, &FragmentShaderCode, NULL);
    glCompileShader(FragmentShader);
    glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &Success);
    if(Success == GL_FALSE)
    {
        glGetShaderInfoLog(FragmentShader, 512, NULL, InfoLog);
        fprintf(stderr, "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED: %s\n", InfoLog);
    }

    int ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, VertexShader);
    glAttachShader(ShaderProgram, FragmentShader);
    glLinkProgram(ShaderProgram);
    glValidateProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
    if(Success == GL_FALSE)
    {
        glGetProgramInfoLog(ShaderProgram, 512, NULL, InfoLog);
        fprintf(stderr, "ERROR::SHADER::PROGRAM::LINKING__FAILED: %s\n", InfoLog);
    }

    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);

    float Vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    while(!Quit)
    {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ShaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        CGLError CGlErr = CGLFlushDrawable(GlContext);
        if(CGlErr != kCGLNoError) {
            printf("CGL Error: %d\n", CGlErr);
        }

        GLenum GlErr = glGetError();
        if(GlErr != GL_NO_ERROR) {
            printf("OpenGL Error: %d\n", GlErr);
        }
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

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
