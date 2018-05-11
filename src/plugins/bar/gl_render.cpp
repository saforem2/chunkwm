#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RENDERER_MAX_SPRITES    20000
#define RENDERER_VERTEX_SIZE    sizeof(gfx_vertex_data)
#define RENDERER_SPRITE_SIZE    RENDERER_VERTEX_SIZE * 4
#define RENDERER_BUFFER_SIZE    RENDERER_SPRITE_SIZE * RENDERER_MAX_SPRITES
#define RENDERER_INDICES_SIZE   RENDERER_MAX_SPRITES * 6
#define RENDERER_TEXTURE_SLOTS  16
#define TRANSFORMATION_STACK_SIZE 20

#define SHADER_VERTEX_INDEX     0
#define SHADER_UV_INDEX         1
#define SHADER_TID_INDEX        2
#define SHADER_COLOR_INDEX      3

#define LAYER_SPRITE_INITCOUNT 20000
#define LAYER_GROUP_INITCOUNT  100
#define GROUP_MAX_SPRITES 200

struct gl_index_buffer
{
    GLuint ID;
    GLuint Count;
};

struct gfx_vertex_data
{
    Vec3 Vertex;
    Vec2 Uv;
    float TID;
    unsigned int Color;
};

struct gfx_texture
{
    char *Filename;
    GLuint ID;
    GLsizei Width;
    GLsizei Height;
    GLsizei Comp;
};

struct gfx_sprite
{
    Vec3 Position;
    Vec2 Size;
    Vec4 Color;
    Vec2 Uv[4];
    gfx_texture *Texture;
    char *Text;
};

struct gfx_renderer
{
    gfx_vertex_data *Buffer;
    GLuint VAO;
    GLuint VBO;
    gl_index_buffer IBO;
    GLsizei IndexCount;

    GLuint *TextureSlots;
    int SlotCount;

    Mat4 *TransformationStack;
    int StackCount;
};

struct gfx_group
{
    gfx_sprite **Sprites;
    int SpriteCount;
    Mat4 TransformationMatrix;
};

struct gfx_layer
{
    gfx_renderer *Renderer;
    gfx_sprite **Sprites;
    int SpriteCount;
    gfx_group **Groups;
    int GroupCount;
    gl_shader *Shader;
    Mat4 ProjectionMatrix;
};

gl_index_buffer GLCreateIndexbuffer(GLushort *Data, GLsizei Count)
{
    gl_index_buffer Buffer;
    Buffer.Count = Count;

    glGenBuffers(1, &Buffer.ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer.ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Count * sizeof(GLushort), Data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return Buffer;;
}

gl_index_buffer GLCreateIndexbuffer(GLuint *Data, GLsizei Count)
{
    gl_index_buffer Buffer;
    Buffer.Count = Count;

    glGenBuffers(1, &Buffer.ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer.ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Count * sizeof(GLuint), Data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return Buffer;;
}

void GLDeleteIndexbuffer(gl_index_buffer *Buffer)
{
    glDeleteBuffers(1, &Buffer->ID);
}

void GLBindIndexbuffer(gl_index_buffer *Buffer)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer->ID);
}

void GLUnbindIndexbuffer(void)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

gfx_sprite GFXCreateSprite(void)
{
    gfx_sprite Sprite = {};

    Sprite.Uv[0].x = 0;
    Sprite.Uv[0].y = 0;

    Sprite.Uv[1].x = 0;
    Sprite.Uv[1].y = 1;

    Sprite.Uv[2].x = 1;
    Sprite.Uv[2].y = 1;

    Sprite.Uv[3].x = 1;
    Sprite.Uv[3].y = 0;

    return Sprite;
}

gfx_sprite GFXCreateSprite(float px, float py, float pz, float sx, float sy, float cx, float cy, float cz, float cw)
{
    gfx_sprite Sprite = GFXCreateSprite();

    Sprite.Position.x = px;
    Sprite.Position.y = py;
    Sprite.Position.z = pz;
    Sprite.Size.x = sx;
    Sprite.Size.y = sy;
    Sprite.Color.x = cx;
    Sprite.Color.y = cy;
    Sprite.Color.z = cz;
    Sprite.Color.w = cw;
    Sprite.Texture = NULL;

    return Sprite;
}

gfx_sprite GFXCreateSprite(float px, float py, float pz, float sx, float sy, float cx, float cy, float cz, float cw, char *text)
{
    gfx_sprite Sprite = GFXCreateSprite();

    Sprite.Position.x = px;
    Sprite.Position.y = py;
    Sprite.Position.z = pz;
    Sprite.Size.x = sx;
    Sprite.Size.y = sy;
    Sprite.Color.x = cx;
    Sprite.Color.y = cy;
    Sprite.Color.z = cz;
    Sprite.Color.w = cw;
    Sprite.Texture = NULL;
    Sprite.Text = strdup(text);

    return Sprite;
}

gfx_sprite GFXCreateSprite(float px, float py, float pz, float sx, float sy, gfx_texture *Texture)
{
    gfx_sprite Sprite = GFXCreateSprite();

    Sprite.Position.x = px;
    Sprite.Position.y = py;
    Sprite.Position.z = pz;
    Sprite.Size.x = sx;
    Sprite.Size.y = sy;

    // NOTE: Fallback color if texture fails
    Sprite.Color.x = 1;
    Sprite.Color.y = 0;
    Sprite.Color.z = 1;
    Sprite.Color.w = 1;
    Sprite.Texture = Texture;

    return Sprite;
}

GLuint GFXGetTextureID(gfx_sprite *Sprite)
{
    return Sprite->Texture == NULL ? 0 : Sprite->Texture->ID;
}

gfx_renderer *GFXCreateRenderer(void)
{
    gfx_renderer *Renderer = (gfx_renderer *) malloc(sizeof(struct gfx_renderer));
    memset(Renderer, 0, sizeof(struct gfx_renderer));

    glGenVertexArrays(1, &Renderer->VAO);
    glGenBuffers(1, &Renderer->VBO);

    glBindVertexArray(Renderer->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, Renderer->VBO);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(SHADER_VERTEX_INDEX);
    glEnableVertexAttribArray(SHADER_UV_INDEX);
    glEnableVertexAttribArray(SHADER_TID_INDEX);
    glEnableVertexAttribArray(SHADER_COLOR_INDEX);

    glVertexAttribPointer(SHADER_VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, RENDERER_VERTEX_SIZE, (const GLvoid*) 0);
    glVertexAttribPointer(SHADER_UV_INDEX, 2, GL_FLOAT, GL_FALSE, RENDERER_VERTEX_SIZE, (const GLvoid*) offsetof(gfx_vertex_data, Uv));
    glVertexAttribPointer(SHADER_TID_INDEX, 1, GL_FLOAT, GL_FALSE, RENDERER_VERTEX_SIZE, (const GLvoid*) offsetof(gfx_vertex_data, TID));
    glVertexAttribPointer(SHADER_COLOR_INDEX, 4, GL_UNSIGNED_BYTE, GL_TRUE, RENDERER_VERTEX_SIZE, (const GLvoid*) offsetof(gfx_vertex_data, Color));

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint indices[RENDERER_INDICES_SIZE];
    int offset = 0;
    for(int i = 0; i < RENDERER_INDICES_SIZE; i+=6)
    {
        indices[i] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;
        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;

        offset += 4;
    }
    Renderer->IBO = GLCreateIndexbuffer(indices, RENDERER_INDICES_SIZE);
    glBindVertexArray(0);

    Renderer->TransformationStack = (Mat4 *) malloc(sizeof(Mat4) * TRANSFORMATION_STACK_SIZE);
    Renderer->TransformationStack[Renderer->StackCount++] = Mat4Identity();

    Renderer->TextureSlots = (GLuint *) malloc(sizeof(GLuint) * RENDERER_TEXTURE_SLOTS);
    Renderer->SlotCount = 0;

    return Renderer;
}

void GFXDeleteRenderer(gfx_renderer *Renderer)
{
    glDeleteBuffers(1, &Renderer->VBO);
    GLDeleteIndexbuffer(&Renderer->IBO);

    free(Renderer->TransformationStack);
    free(Renderer->TextureSlots);
    free(Renderer);
}

void GFXRendererBegin(gfx_renderer *Renderer)
{
    glBindBuffer(GL_ARRAY_BUFFER, Renderer->VBO);
    Renderer->Buffer = (gfx_vertex_data *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
}

void GFXRendererEnd(void)
{
    glUnmapBuffer(GL_ARRAY_BUFFER);
}

void GFXFlushRenderer(gfx_renderer *Renderer)
{
    for(int SlotIndex = 0; SlotIndex < Renderer->SlotCount; ++SlotIndex)
    {
        glActiveTexture(GL_TEXTURE0 + SlotIndex);
        glBindTexture(GL_TEXTURE_2D, Renderer->TextureSlots[SlotIndex]);
    }

    glBindVertexArray(Renderer->VAO);
    GLBindIndexbuffer(&Renderer->IBO);

    glDrawElements(GL_TRIANGLES, Renderer->IndexCount, GL_UNSIGNED_INT, NULL);

    GLUnbindIndexbuffer();
    glBindVertexArray(0);

    Renderer->IndexCount = 0;
}

void GFXSubmitToRenderer(gfx_renderer *Renderer, gfx_sprite *Sprite)
{
    Vec3 *Position = &Sprite->Position;
    Vec2 *Size = &Sprite->Size;
    Vec4 *Color = &Sprite->Color;
    Vec2 *Uv = Sprite->Uv;
    GLuint TID = GFXGetTextureID(Sprite);

    unsigned int c = 0;
    float ts = 0.0f;
    if(TID > 0)
    {
        bool found = false;
        for(int SlotIndex = 0; SlotIndex < Renderer->SlotCount; ++SlotIndex)
        {
            if(Renderer->TextureSlots[SlotIndex] == TID)
            {
                ts = (float) SlotIndex + 1;
                found = true;
                break;
            }
        }

        if(!found)
        {
            if(Renderer->SlotCount >= RENDERER_TEXTURE_SLOTS)
            {
                GFXRendererEnd();
                GFXFlushRenderer(Renderer);
                GFXRendererBegin(Renderer);
                Renderer->SlotCount = 0;
            }
            Renderer->TextureSlots[Renderer->SlotCount] = TID;
            Renderer->SlotCount++;
            ts = (float) Renderer->SlotCount;
        }
    }
    else
    {
        int r = Color->x * 255.0f;
        int g = Color->y * 255.0f;
        int b = Color->z * 255.0f;
        int a = Color->w * 255.0f;

        c = a << 24 | b << 16 | g << 8 | r;
    }

    Mat4 *HeadOfStack = &Renderer->TransformationStack[Renderer->StackCount - 1];

    Renderer->Buffer->Vertex = Mat4Multiply(HeadOfStack, Position->x, Position->y, 0);
    Renderer->Buffer->Uv = Uv[0];
    Renderer->Buffer->TID = ts;
    Renderer->Buffer->Color = c;
    Renderer->Buffer++;

    Renderer->Buffer->Vertex = Mat4Multiply(HeadOfStack, Position->x, Position->y + Size->y, 0);
    Renderer->Buffer->Uv = Uv[1];
    Renderer->Buffer->TID = ts;
    Renderer->Buffer->Color = c;
    Renderer->Buffer++;

    Renderer->Buffer->Vertex = Mat4Multiply(HeadOfStack, Position->x + Size->x, Position->y + Size->y, 0);
    Renderer->Buffer->Uv = Uv[2];
    Renderer->Buffer->TID = ts;
    Renderer->Buffer->Color = c;
    Renderer->Buffer++;

    Renderer->Buffer->Vertex = Mat4Multiply(HeadOfStack, Position->x + Size->x, Position->y, 0);
    Renderer->Buffer->Uv = Uv[3];
    Renderer->Buffer->TID = ts;
    Renderer->Buffer->Color = c;
    Renderer->Buffer++;

    Renderer->IndexCount += 6;
}

void GFXDrawString(gfx_renderer *Renderer, gfx_sprite *Sprite)
{
}


void GFXAddToTransformationStack(gfx_renderer *Renderer, Mat4 Matrix)
{
    if(Renderer->StackCount < TRANSFORMATION_STACK_SIZE)
    {
        memcpy(Renderer->TransformationStack[Renderer->StackCount].Elements, Matrix.Elements, sizeof(float) * 16);
        Renderer->StackCount++;
    }
    else
    {
        // TODO: RESIZE STACK
    }
}

void GFXPopTransformationStack(gfx_renderer *Renderer)
{
    if(Renderer->StackCount > 1)
        Renderer->StackCount--;
}

gfx_group GFXCreateGroup(Mat4 Transform)
{
    gfx_group Group;

    Group.TransformationMatrix = Transform;
    Group.Sprites = (gfx_sprite **) malloc(sizeof(gfx_sprite) * GROUP_MAX_SPRITES);
    Group.SpriteCount = 0;

    return Group;
}

gfx_group GFXCreateGroup(void)
{
    gfx_group Group;

    Group.TransformationMatrix = Mat4Identity();
    Group.Sprites = (gfx_sprite **) malloc(sizeof(gfx_sprite) * GROUP_MAX_SPRITES);
    Group.SpriteCount = 0;

    return Group;
}

void GFXDeleteGroup(gfx_group *Group)
{
    free(Group->Sprites);
}

void GFXAddToGroup(gfx_group *Group, gfx_sprite *Sprite)
{
    if(Group->SpriteCount < GROUP_MAX_SPRITES)
        Group->Sprites[Group->SpriteCount++] = Sprite;
}

void GFXSubmitGroupToRenderer(gfx_renderer *Renderer, gfx_group *Group)
{
    GFXAddToTransformationStack(Renderer, Group->TransformationMatrix);
    for(int SpriteIndex = 0; SpriteIndex < Group->SpriteCount; ++SpriteIndex)
        GFXSubmitToRenderer(Renderer, Group->Sprites[SpriteIndex]);

    GFXPopTransformationStack(Renderer);
}

gfx_layer * GFXCreateLayer(gfx_renderer *Renderer, gl_shader *Shader, Mat4 ProjectionMatrix)
{
    gfx_layer *Layer = (gfx_layer *) malloc(sizeof(gfx_layer));

    Layer->Renderer = Renderer;
    Layer->Shader = Shader;
    Layer->Sprites = (gfx_sprite**) malloc(sizeof(gfx_sprite*) * LAYER_SPRITE_INITCOUNT);
    Layer->SpriteCount = 0;
    Layer->Groups = (gfx_group **) malloc(sizeof(gfx_group*) * LAYER_GROUP_INITCOUNT);
    Layer->GroupCount = 0;
    Layer->ProjectionMatrix = ProjectionMatrix;

    GLEnableShader(Layer->Shader);
    GLShaderSetUniformMat4(Layer->Shader, "pr_matrix", Layer->ProjectionMatrix.Elements);
    GLDisableShader();
    return Layer;
}

// NOTE: Sprites should NOT be freed by layer or group
// Layer SHOULD delete groups though
void GFXDeleteLayer(gfx_layer *Layer)
{
    GLDeleteShader(Layer->Shader);

    for(int GroupIndex = 0; GroupIndex < Layer->GroupCount; ++GroupIndex)
        GFXDeleteGroup(Layer->Groups[GroupIndex]);

    free(Layer->Groups);
    free(Layer->Sprites);
    free(Layer);
}

void GFXAddToLayer(gfx_layer *Layer, gfx_sprite *Sprite)
{
    Layer->Sprites[Layer->SpriteCount++] = Sprite;
}

void GFXAddToLayer(gfx_layer *Layer, gfx_group *Group)
{
    Layer->Groups[Layer->GroupCount++] = Group;
}

void GFXRenderLayer(gfx_layer *Layer)
{
    GLEnableShader(Layer->Shader);
    GFXRendererBegin(Layer->Renderer);

    for(int SpriteIndex = 0; SpriteIndex < Layer->SpriteCount; ++SpriteIndex)
        GFXSubmitToRenderer(Layer->Renderer, Layer->Sprites[SpriteIndex]);

    for(int GroupIndex = 0; GroupIndex < Layer->GroupCount; ++GroupIndex)
        GFXSubmitGroupToRenderer(Layer->Renderer, Layer->Groups[GroupIndex]);

    GFXRendererEnd();
    GFXFlushRenderer(Layer->Renderer);
}

gfx_texture GFXCreateTexture(const char *Filename)
{
    gfx_texture Texture;
    Texture.Filename = (char *) malloc(strlen(Filename) + 1);
    strcpy(Texture.Filename, Filename);
    return Texture;
}

void GFXDeleteTexture(gfx_texture *Texture)
{
    free(Texture->Filename);
}

void GFXLoadTexture(gfx_texture *Texture)
{
    stbi_set_flip_vertically_on_load(true);
    unsigned char *Pixels = stbi_load(Texture->Filename, &Texture->Width, &Texture->Height, &Texture->Comp, STBI_rgb_alpha);
    if(Pixels)
    {
        glGenTextures(1, &Texture->ID);
        glBindTexture(GL_TEXTURE_2D, Texture->ID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Texture->Width, Texture->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(Pixels);
    }
}

void GLBindTexture(gfx_texture *Texture)
{
    glBindTexture(GL_TEXTURE_2D, Texture->ID);
}

void GLUnbindTexture(void)
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
