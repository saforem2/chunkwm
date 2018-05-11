#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct gl_shader
{
    GLuint ID;
};

GLint GLShaderGetUniformLocation(gl_shader  *Shader, const GLchar *Name)
{
    return glGetUniformLocation(Shader->ID, Name);
}

void GLShaderSetUniform1f(gl_shader *Shader, const GLchar *Name, float Value)
{
    glUniform1f(GLShaderGetUniformLocation(Shader, Name), Value);
}

void GLShaderSetUniform1fv(gl_shader *Shader, const GLchar *Name, float *Value, int Count)
{
    glUniform1fv(GLShaderGetUniformLocation(Shader, Name), Count, Value);
}

void GLShaderSetUniform1i(gl_shader *Shader, const GLchar *Name, int Value)
{
    glUniform1i(GLShaderGetUniformLocation(Shader, Name), Value);
}

void GLShaderSetUniform1iv(gl_shader *Shader, const GLchar *Name, int *Value, int Count)
{
    glUniform1iv(GLShaderGetUniformLocation(Shader, Name), Count, Value);
}

void GLShaderSetUniform2f(gl_shader *Shader, const GLchar *Name, float x, float y)
{
    glUniform2f(GLShaderGetUniformLocation(Shader, Name), x, y);
}

void GLShaderSetUniform3f(gl_shader *Shader, const GLchar *Name, float x, float y, float z)
{
    glUniform3f(GLShaderGetUniformLocation(Shader, Name), x, y, z);
}

void GLShaderSetUniform4f(gl_shader *Shader, const GLchar *Name, float x, float y, float z, float w)
{
    glUniform4f(GLShaderGetUniformLocation(Shader, Name), x, y, z, w);
}

void GLShaderSetUniformMat4(gl_shader *Shader, const GLchar *Name, float *Elements)
{
    glUniformMatrix4fv(GLShaderGetUniformLocation(Shader, Name), 1, GL_FALSE, Elements);
}

gl_shader GLCreateShader(const char *VertexCode, const char *FragmentCode)
{
    gl_shader Shader;

    GLuint program = glCreateProgram();
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertex, 1, &VertexCode, NULL);
    glCompileShader(vertex);

    GLint result;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &result);
    if(result == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(vertex, GL_INFO_LOG_LENGTH, &length);
        char error[length];
        glGetShaderInfoLog(vertex, length, &length, error);
        printf("%s\n", error);
        glDeleteShader(vertex);
        return Shader;
    }

    glShaderSource(fragment, 1, &FragmentCode, NULL);
    glCompileShader(fragment);

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &result);
    if(result == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(fragment, GL_INFO_LOG_LENGTH, &length);
        char error[length];
        glGetShaderInfoLog(fragment, length, &length, error);
        printf("%s\n", error);
        glDeleteShader(fragment);
        return Shader;
    }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    Shader.ID = program;

    return Shader;
}

void GLDeleteShader(gl_shader *Shader)
{
    glDeleteProgram(Shader->ID);
}

void GLEnableShader(gl_shader *Shader)
{
    glUseProgram(Shader->ID);
}

void GLDisableShader(void)
{
    glUseProgram(0);
}
