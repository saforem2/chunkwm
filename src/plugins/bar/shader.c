#include "shader.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <stdio.h>

#if 0
static char *
read_file(const char *file)
{
    // TODO(koekeishiya): NYI
    return NULL;
}
#endif

static void
link_gl_program(struct shader *shader, int vertex_shader, int fragment_shader)
{
    GLint success;
    char log[512];

    glAttachShader(shader->id, vertex_shader);
    glAttachShader(shader->id, fragment_shader);
    glLinkProgram(shader->id);
    glValidateProgram(shader->id);
    glGetProgramiv(shader->id, GL_LINK_STATUS, &success);

    if(success == GL_FALSE)
    {
        glGetProgramInfoLog(shader->id, sizeof(log), NULL, log);
        fprintf(stderr, "error: shader program linking failed with message '%s'\n", log);
    }
}

static void
compile_gl_shader(int shader, const char *buffer)
{
    GLint success;
    char log[512];

    glShaderSource(shader, 1, &buffer, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "error: shader compilation failed with message '%s'\n", log);
    }
}

void shader_init_buffer(struct shader *shader, const char *vertex_buffer, const char *fragment_buffer)
{
    shader->id = glCreateProgram();

    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    compile_gl_shader(vertex_shader, vertex_buffer);
    compile_gl_shader(fragment_shader, fragment_buffer);

    link_gl_program(shader, vertex_shader, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

#if 0
void shader_init_file(struct shader *shader, const char *vertex_path, const char *fragment_path)
{
    // TODO(koekeishiya): MISSING read_file IMPL !!
    const char *vertex_buffer = read_file(vertex_path);
    const char *fragment_buffer = read_file(fragment_path);

    shader_init_buffer(shader, vertex.buffer, fragment.buffer);

    free(vertex.buffer);
    free(fragment.buffer);
}
#endif

void shader_enable(struct shader *shader)
{
    glUseProgram(shader->id);
}

void shader_disable()
{
    glUseProgram(0);
}
