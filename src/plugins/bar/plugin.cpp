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
#include "../../common/config/cvar.cpp"

#include "cgl_window.h"
#include "cgl_window.c"

#include "shader.h"
#include "shader.c"

#define internal static

internal const char *plugin_name = "bar";
internal const char *plugin_version = "0.0.1";
internal chunkwm_api api;

internal struct cgl_window mid_window;
internal struct cgl_window left_window;
internal struct cgl_window right_window;
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

float width_of_rendered_text(const char *text, float sx)
{
    float x = 0.0f;
    float total_width = 0.0f;
    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) {
            continue;
        }

        total_width += face->glyph->bitmap.width * sx;
        if (p[1]) x += (face->glyph->advance.x / 192) * sx;
    }
    return total_width + x;
}

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

internal void
get_current_date(char *buffer, int buffer_size)
{
    time_t time_epoch = time(NULL);
    struct tm *local_time = localtime(&time_epoch);
    buffer[strftime(buffer, buffer_size, "%b %d", local_time)] = '\0';
}

internal void
get_current_time(char *buffer, int buffer_size)
{
    time_t time_epoch = time(NULL);
    struct tm *local_time = localtime(&time_epoch);
    buffer[strftime(buffer, buffer_size, "%H:%M:%S", local_time)] = '\0';
}

float hexf(uint32_t hex)
{
    return hex / 255.0f;
}

struct window_render_state
{
    struct  cgl_window *window;
    GLuint vao, vbo, tex;
};

internal void
init_gl_for_window(struct window_render_state *render_state)
{
    cgl_window_make_current(render_state->window);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &render_state->tex);
    glBindTexture(GL_TEXTURE_2D, render_state->tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenVertexArrays(1, &render_state->vao);
    glGenBuffers(1, &render_state->vbo);

    glBindVertexArray(render_state->vao);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, render_state->vbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glBindVertexArray(0);
}

internal char *focused_application;
void *BarMainThreadProcedure(void*)
{
    GLint cgl_major, cgl_minor;
    CGLError cgl_err;
    GLenum gl_err;

    struct shader text_shader;
    GLfloat color1[4] = {hexf(0xcd), hexf(0x95), hexf(0x0d), 1};
    GLfloat color2[4] = {hexf(0xd7), hexf(0x5f), hexf(0x5f), 1};
    GLfloat color3[4] = {hexf(0x22), hexf(0xc3), hexf(0xa1), 1};

    struct window_render_state left_state = { &left_window };
    init_gl_for_window(&left_state);
    shader_init_buffer(&text_shader, vertex_shader_code, fragment_shader_code);

    struct window_render_state mid_state = { &mid_window };
    init_gl_for_window(&mid_state);
    shader_init_buffer(&text_shader, vertex_shader_code, fragment_shader_code);

    struct window_render_state right_state = { &right_window };
    init_gl_for_window(&right_state);
    shader_init_buffer(&text_shader, vertex_shader_code, fragment_shader_code);

    CGLGetVersion(&cgl_major, &cgl_minor);
    printf("CGL Version: %d.%d\nOpenGL Version: %s\n",
           cgl_major, cgl_minor, glGetString(GL_VERSION));

    // NOTE(koekeishiya): wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    AXUIElementRef application = AXLibGetFocusedApplication();
    if (application) {
        CFTypeRef title_ref = AXLibGetWindowProperty(application, kAXTitleAttribute);
        if (title_ref) {
            focused_application = CopyCFStringToC((CFStringRef)title_ref);
            CFRelease(title_ref);
        }
        CFRelease(application);
    }

    char buffer[128];
    float width_of_buffer;
    float sx;
    float sy;

    while (!quit) {
        // left window
        sx = 2.0f / left_window.width;
        sy = 2.0f / left_window.height;
        cgl_window_make_current(&left_window);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_enable(&text_shader);
        glBindVertexArray(left_state.vao);

        get_current_date(buffer, sizeof(buffer));
        width_of_buffer = width_of_rendered_text(buffer, sx);
        render_text(buffer, 0.0f - width_of_buffer / 2.0f, -0.3f, sx, sy, color2);

        glBindVertexArray(0);
        shader_disable();

        // mid window
        sx = 2.0f / mid_window.width;
        sy = 2.0f / mid_window.height;
        cgl_window_make_current(&mid_window);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_enable(&text_shader);
        glBindVertexArray(mid_state.vao);

        if (focused_application) {
            width_of_buffer = width_of_rendered_text(focused_application, sx);
            render_text(focused_application, 0.0f - width_of_buffer / 2.0f, -0.3f, sx, sy, color1);
        }

        glBindVertexArray(0);
        shader_disable();

        // right window
        sx = 2.0f / right_window.width;
        sy = 2.0f / right_window.height;
        cgl_window_make_current(&right_window);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_enable(&text_shader);
        glBindVertexArray(right_state.vao);

        get_current_time(buffer, sizeof(buffer));
        width_of_buffer = width_of_rendered_text(buffer, sx);
        render_text(buffer, 0.0f - width_of_buffer / 2.0f, -0.3f, sx, sy, color3);

        glBindVertexArray(0);
        shader_disable();

        // flush all windows
        if ((cgl_err = cgl_window_flush(&left_window)) != kCGLNoError) {
            printf("CGL Error: %d\n", cgl_err);
        }

        if ((cgl_err = cgl_window_flush(&mid_window)) != kCGLNoError) {
            printf("CGL Error: %d\n", cgl_err);
        }

        if ((cgl_err = cgl_window_flush(&right_window)) != kCGLNoError) {
            printf("CGL Error: %d\n", cgl_err);
        }

        if ((gl_err = glGetError()) != GL_NO_ERROR) {
            printf("OpenGL Error: %d\n", gl_err);
        }

        sleep(1);
    }

    glDeleteVertexArrays(1, &left_state.vao);
    glDeleteBuffers(1, &left_state.vbo);
    glDeleteVertexArrays(1, &mid_state.vao);
    glDeleteBuffers(1, &mid_state.vbo);
    glDeleteVertexArrays(1, &right_state.vao);
    glDeleteBuffers(1, &right_state.vbo);

    return NULL;
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if (strcmp(Node, "chunkwm_export_application_activated") == 0) {
        macos_application *application = (macos_application *) Data;
        if (focused_application) free(focused_application);
        focused_application = strdup(application->Name);
        return true;
    }
    return false;
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    api = ChunkwmAPI;
    BeginCVars(&api);

    CreateCVar("bar_font", "/Library/Fonts/Georgia.ttf");

    int left_x = 15;
    int left_width = 100;

    int right_x = 1805;
    int right_width = 100;

    int x = 510;
    int y = 15;
    int width = 900;
    int height = 40;
    int font_size = 18;

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init freetype library\n");
        return false;
    }

    if (FT_New_Face(ft, CVarStringValue("bar_font"), 0, &face)) {
        fprintf(stderr, "Could not open font\n");
        return false;
    }

    if (!cgl_window_init(&left_window, left_x, y, left_width, height, kCGMaximumWindowLevelKey, CGL_WINDOW_GL_CORE, 1)) {
        return false;
    }

    if (!cgl_window_init(&mid_window, x, y, width, height, kCGMaximumWindowLevelKey, CGL_WINDOW_GL_CORE, 1)) {
        return false;
    }

    if (!cgl_window_init(&right_window, right_x, y, right_width, height, kCGMaximumWindowLevelKey, CGL_WINDOW_GL_CORE, 1)) {
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, font_size);
    cgl_window_set_sticky(&left_window, 1);
    cgl_window_set_sticky(&mid_window, 1);
    cgl_window_set_sticky(&right_window, 1);
    pthread_create(&bar_thread, NULL, &BarMainThreadProcedure, NULL);
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    quit = true;
    pthread_join(bar_thread, NULL);
    cgl_window_destroy(&left_window);
    cgl_window_destroy(&mid_window);
    cgl_window_destroy(&right_window);
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export subscriptions[] =
{
    chunkwm_export_application_activated,
};
CHUNKWM_PLUGIN_SUBSCRIBE(subscriptions)
CHUNKWM_PLUGIN(plugin_name, plugin_version);
