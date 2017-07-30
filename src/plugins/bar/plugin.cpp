#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <pthread.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"

#include "cgl_window.h"
#include "cgl_window.c"

#include "shader.h"
#include "shader.c"

#define internal static

internal const char *PluginName = "bar";
internal const char *PluginVersion = "0.0.1";
internal chunkwm_api API;

internal struct cgl_window Window;
internal pthread_t BarThread;
internal bool Quit;

const char *VertexShaderCode =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 oColor;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "   oColor = aColor;\n"
    "}\n\0";

const char *FragmentShaderCode =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 oColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(oColor, 1.0f);\n"
    "}\n\0";

void *BarMainThreadProcedure(void*)
{
    struct shader ShaderProgram;

    CGLError CGlErr;
    GLenum GlErr;

    GLint CGLMajor, CGLMinor;
    GLuint VAO, VBO;

    float Vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.2f, 1.0f, 0.2f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.2f,
         0.0f,  0.5f, 0.0f, 0.2f, 0.2f, 1.0f
    };

    cgl_window_make_current(&Window);

    CGLGetVersion(&CGLMajor, &CGLMinor);
    printf("CGL Version: %d.%d\nOpenGL Version: %s\n",
           CGLMajor, CGLMinor, glGetString(GL_VERSION));

    // NOTE(koekeishiya): wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    shader_init_buffer(&ShaderProgram, VertexShaderCode, FragmentShaderCode);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    while(!Quit)
    {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_enable(&ShaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        shader_disable();

        if((CGlErr = cgl_window_flush(&Window)) != kCGLNoError)
        {
            printf("CGL Error: %d\n", CGlErr);
        }

        if((GlErr = glGetError()) != GL_NO_ERROR)
        {
            printf("OpenGL Error: %d\n", GlErr);
        }

        sleep(1);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    return NULL;
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    return false;
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;

    int x = 0;
    int y = 22;
    int width = 500;
    int height = 500;
    int level = kCGMaximumWindowLevelKey;

    if(!cgl_window_init(&Window, x, y, width, height, level))
    {
        return false;
    }

    pthread_create(&BarThread, NULL, &BarMainThreadProcedure, NULL);
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    Quit = true;
    pthread_join(BarThread, NULL);
    cgl_window_destroy(&Window);
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] = { };
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN(PluginName, PluginVersion);
