#ifndef SHADER_H
#define SHADER_H

struct shader
{
    int id;
};

void shader_init_file(struct shader *shader, const char *vertex_path, const char *fragment_path);
void shader_init_buffer(struct shader *shader, const char *vertex_buffer, const char *fragment_buffer);
void shader_enable(struct shader *shader);
void shader_disable();

#endif
