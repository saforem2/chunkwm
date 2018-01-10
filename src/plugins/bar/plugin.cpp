#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Carbon/Carbon.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <pthread.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/window.cpp"
#include "../../common/accessibility/element.cpp"

#include "cgl_window.h"
#include "cgl_window.c"

#include "shader.h"
#include "shader.c"

#define internal static

internal const char *plugin_name = "bar";
internal const char *plugin_version = "0.0.1";
internal chunkwm_api api;

internal struct cgl_window window;
internal pthread_t bar_thread;
internal bool quit;

internal FT_Library ft;
internal FT_Face face;

const char *vertex_shader_code =
    "#version 330 core\n"
    "layout (location = 0) in vec4 coord;\n"
    "out vec2 tex_coord;\n"
    "void main(void) {\n"
    "  gl_Position = vec4(coord.xy, 0, 1);\n"
    "  tex_coord = coord.zw;\n"
    "}\n\0";

const char *fragment_shader_code =
    "#version 330 core\n"
    "in vec2 tex_coord;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D tex;\n"
    "uniform vec4 color;\n"
    "void main(void) {\n"
    "  frag_color = vec4(1, 1, 1, texture(tex, tex_coord).r) * color;\n"
    "}\n\0";

void render_text(const char *text, float x, float y, float sx, float sy, GLfloat *color) {
    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) {
            continue;
        }

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RED,
                     face->glyph->bitmap.width,
                     face->glyph->bitmap.rows,
                     0,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     face->glyph->bitmap.buffer);

        float x2 = x + face->glyph->bitmap_left * sx;
        float y2 = -y - face->glyph->bitmap_top * sy;
        float w = face->glyph->bitmap.width * sx;
        float h = face->glyph->bitmap.rows * sy;

        GLfloat box[4][4] = {
            {x2, -y2, 0, 0},
            {x2 + w, -y2, 1, 0},
            {x2, -y2 - h, 0, 1},
            {x2 + w, -y2 - h, 1, 1},
        };

        glUniform4fv(1, 1, color);
        glBufferData(GL_ARRAY_BUFFER, sizeof(box), box, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        x += (face->glyph->advance.x / 64) * sx;
        y += (face->glyph->advance.y / 64) * sy;
    }
}

internal char *text;
void *BarMainThreadProcedure(void*)
{
    CGLError cgl_err;
    GLenum gl_err;

    GLint cgl_major, cgl_minor;
    GLuint vao, vbo, tex;

    struct shader text_shader;
    GLfloat color1[4] = {0.75, 0.75, 0.30, 1};
    GLfloat color2[4] = {0.57, 0.79, 0.79, 1};

    float sx = 2.0 / window.width;
    float sy = 2.0 / window.height;

    cgl_window_make_current(&window);
    CGLGetVersion(&cgl_major, &cgl_minor);
    printf("CGL Version: %d.%d\nOpenGL Version: %s\n",
           cgl_major, cgl_minor, glGetString(GL_VERSION));

    // NOTE(koekeishiya): wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    shader_init_buffer(&text_shader, vertex_shader_code, fragment_shader_code);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glBindVertexArray(0);

    while (!quit) {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_enable(&text_shader);
        glBindVertexArray(vao);

        // char text[256] = {};
        // snprintf(text, sizeof(text), "sx = %.2f and sy = %.2f å", sx, sy);
        if (text) {
            render_text(text, -0.95f, -0.3, sx, sy, color2);
        }

        glBindVertexArray(0);
        shader_disable();

        if ((cgl_err = cgl_window_flush(&window)) != kCGLNoError) {
            printf("CGL Error: %d\n", cgl_err);
        }

        if ((gl_err = glGetError()) != GL_NO_ERROR) {
            printf("OpenGL Error: %d\n", gl_err);
        }

        sleep(1);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    return NULL;
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if (strcmp(Node, "chunkwm_export_application_activated") == 0) {
        macos_application *application = (macos_application *) Data;
        text = strdup(application->Name);
        return true;
    } else if (strcmp(Node, "chunkwm_export_window_focused") == 0) {
        macos_window *window = (macos_window *) Data;
        text = strdup(window->Name);
        return true;
    }
    return false;
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    api = ChunkwmAPI;

    int x = 0;
    int y = 0;
    int width = 600;
    int height = 20;
    int font_size = 12;

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init freetype library\n");
        return false;
    }

    if (FT_New_Face(ft, "/Library/Fonts/Courier New.ttf", 0, &face)) {
        fprintf(stderr, "Could not open font\n");
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, font_size);

    if (!cgl_window_init(&window, x, y, width, height, kCGMaximumWindowLevelKey)) {
        return false;
    }

    pthread_create(&bar_thread, NULL, &BarMainThreadProcedure, NULL);
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    quit = true;
    pthread_join(bar_thread, NULL);
    cgl_window_destroy(&window);
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export subscriptions[] =
{
    chunkwm_export_window_focused,
    chunkwm_export_application_activated,
};
CHUNKWM_PLUGIN_SUBSCRIBE(subscriptions)
CHUNKWM_PLUGIN(plugin_name, plugin_version);
