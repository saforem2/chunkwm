#include <string.h>
#include <math.h>

struct Vec2
{
    float x;
    float y;
};

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Vec4
{
    float x;
    float y;
    float z;
    float w;
};

struct Mat4
{
    union
    {
        float Elements[4 * 4];
        Vec4 Columns[4];
    };
};

static float
ToRadians(float Degrees)
{
    return Degrees * (M_PI / 180.0f);
}

Vec2 Vec2New(float x, float y)
{
    Vec2 Result;
    Result.x = x;
    Result.y = y;
    return Result;
}

Vec2 * Vec2Add(Vec2 *Result, Vec2 *Other)
{
    Result->x += Other->x;
    Result->y += Other->y;
    return Result;
}

Vec2 * Vec2Subtract(Vec2 *Result, Vec2 *Other)
{
    Result->x -= Other->x;
    Result->y -= Other->y;
    return Result;
}

Vec2 * Vec2Multiply(Vec2 *Result, Vec2 *Other)
{
    Result->x *= Other->x;
    Result->y *= Other->y;
    return Result;
}

Vec2 * Vec2Divide(Vec2 *Result, Vec2 *Other)
{
    Result->x /= Other->x;
    Result->y /= Other->y;
    return Result;
}

bool Vec2IsEqual(Vec2 *Left, Vec2 *Right)
{
    return Left->x == Right->x &&
           Left->y == Right->y;
}

Vec3 Vec3New(float x, float y, float z)
{
    Vec3 Result;
    Result.x = x;
    Result.y = y;
    Result.z = z;
    return Result;
}

Vec3 * Vec3Add(Vec3 *Result, Vec3 *Other)
{
    Result->x += Other->x;
    Result->y += Other->y;
    Result->z += Other->z;
    return Result;
}

Vec3 * Vec3Subtract(Vec3 *Result, Vec3 *Other)
{
    Result->x -= Other->x;
    Result->y -= Other->y;
    Result->z -= Other->z;
    return Result;
}

Vec3 * Vec3Multiply(Vec3 *Result, Vec3 *Other)
{
    Result->x *= Other->x;
    Result->y *= Other->y;
    Result->z *= Other->z;
    return Result;
}

Vec3 * Vec3Divide(Vec3 *Result, Vec3 *Other)
{
    Result->x /= Other->x;
    Result->y /= Other->y;
    Result->z /= Other->z;
    return Result;
}

bool Vec3IsEqual(Vec3 *Left, Vec3 *Right)
{
    return Left->x == Right->x &&
           Left->y == Right->y &&
           Left->z == Right->z;
}


Vec4 Vec4New(float x, float y, float z, float w)
{
    Vec4 Result;
    Result.x = x;
    Result.y = y;
    Result.z = z;
    Result.w = w;
    return Result;
}

Vec4 * Vec4Add(Vec4 *Result, Vec4 *Other)
{
    Result->x += Other->x;
    Result->y += Other->y;
    Result->z += Other->z;
    Result->w += Other->w;
    return Result;
}

Vec4 * Vec4Subtract(Vec4 *Result, Vec4 *Other)
{
    Result->x -= Other->x;
    Result->y -= Other->y;
    Result->z -= Other->z;
    Result->w -= Other->w;
    return Result;
}

Vec4 * Vec4Multiply(Vec4 *Result, Vec4 *Other)
{
    Result->x *= Other->x;
    Result->y *= Other->y;
    Result->z *= Other->z;
    Result->w *= Other->w;
    return Result;
}

Vec4 * Vec4Divide(Vec4 *Result, Vec4 *Other)
{
    Result->x /= Other->x;
    Result->y /= Other->y;
    Result->z /= Other->z;
    Result->w /= Other->w;
    return Result;
}

bool Vec4IsEqual(Vec4 *Left, Vec4 *Right)
{
    return Left->x == Right->x &&
           Left->y == Right->y &&
           Left->z == Right->z &&
           Left->w == Right->w;
}

Mat4 *Mat4Multiply(Mat4 *Result, Mat4 *Other)
{
    float Data[16];
    for(int y = 0; y < 4; ++y)
    {
        for(int x = 0; x < 4; ++x)
        {
            float sum = 0.0f;
            for(int e = 0; e < 4; ++e)
                sum += Result->Elements[x + e * 4] * Other->Elements[e + y * 4];

            Data[x + y * 4] = sum;
        }
    }
    memcpy(Result->Elements, Data, sizeof(float) * 16);
    return Result;
}

Vec3 Mat4Multiply(Mat4 *Matrix, float x, float y, float z)
{
    Vec3 Result;
    Result.x = Matrix->Columns[0].x * x + Matrix->Columns[1].x * y + Matrix->Columns[2].x * z + Matrix->Columns[3].x;
    Result.y = Matrix->Columns[0].y * x + Matrix->Columns[1].y * y + Matrix->Columns[2].y * z + Matrix->Columns[3].y;
    Result.z = Matrix->Columns[0].z * x + Matrix->Columns[1].z * y + Matrix->Columns[2].z * z + Matrix->Columns[3].z;
    return Result;
}
Vec4 Mat4Multiply(Mat4 *Matrix, float x, float y, float z, float w)
{
    Vec4 Result;
    Result.x = Matrix->Columns[0].x * x + Matrix->Columns[1].x * y + Matrix->Columns[2].x * z + Matrix->Columns[3].x * w;
    Result.y = Matrix->Columns[0].y * x + Matrix->Columns[1].y * y + Matrix->Columns[2].y * z + Matrix->Columns[3].y * w;
    Result.z = Matrix->Columns[0].z * x + Matrix->Columns[1].z * y + Matrix->Columns[2].z * z + Matrix->Columns[3].z * w;
    Result.w = Matrix->Columns[0].w * x + Matrix->Columns[1].w * y + Matrix->Columns[2].w * z + Matrix->Columns[3].w * w;
    return Result;
}

void Mat4Clear(Mat4 *Result)
{
    memset(Result, 0, sizeof(Mat4));
}

void Mat4Diagonal(Mat4 *Result, float Diagonal)
{
    Mat4Clear(Result);
    Result->Elements[0 + 0 * 4] = Diagonal;
    Result->Elements[1 + 1 * 4] = Diagonal;
    Result->Elements[2 + 2 * 4] = Diagonal;
    Result->Elements[3 + 3 * 4] = Diagonal;
}

Mat4 Mat4Identity(void)
{
    Mat4 Identity;
    Mat4Diagonal(&Identity, 1.0f);
    return Identity;
}

Mat4 Mat4Orthographic(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    Mat4 Orthographic;
    Mat4Diagonal(&Orthographic, 1.0f);

    Orthographic.Elements[0 + 0 * 4] = 2.0f / (Right - Left);
    Orthographic.Elements[1 + 1 * 4] = 2.0f / (Top - Bottom);;
    Orthographic.Elements[2 + 2 * 4] = 2.0f / (Near - Far);;

    Orthographic.Elements[0 + 3 * 4] = (Left + Right) / (Left - Right);
    Orthographic.Elements[1 + 3 * 4] = (Bottom + Top) / (Bottom - Top);
    Orthographic.Elements[2 + 3 * 4] = (Far + Near) / (Far - Near);

    return Orthographic;
}

Mat4 Mat4Perspective(float Fov, float AspectRatio, float Near, float Far)
{
    Mat4 Perspective;
    Mat4Diagonal(&Perspective, 1.0f);

    float q = 1.0f / tan(ToRadians(0.5f * Fov));
    float a = q / AspectRatio;
    float b = (Near + Far) / (Near - Far);
    float c = (2.0f * Near * Far) / (Near - Far);

    Perspective.Elements[0 + 0 * 4] = a;
    Perspective.Elements[1 + 1 * 4] = q;
    Perspective.Elements[2 + 2 * 4] = b;
    Perspective.Elements[3 + 2 * 4] = -1.0f;
    Perspective.Elements[2 + 3 * 4] = c;

    return Perspective;
}

Mat4 Mat4Translation(float x, float y, float z)
{
    Mat4 Translation;
    Mat4Diagonal(&Translation, 1.0f);

    Translation.Elements[0 + 3 * 4] = x;
    Translation.Elements[1 + 3 * 4] = y;
    Translation.Elements[2 + 3 * 4] = z;

    return Translation;
}

Mat4 Mat4Rotation(float Angle, float x, float y, float z)
{
    Mat4 Rotation;
    Mat4Diagonal(&Rotation, 1.0f);

    float r = ToRadians(Angle);
    float c = cos(r);
    float s = sin(r);
    float omc = 1.0f - c;

    Rotation.Elements[0 + 0* 4] = x * omc + c;
    Rotation.Elements[1 + 0* 4] = y * x * omc + z * s;
    Rotation.Elements[2 + 0* 4] = x * z * omc - y * s;

    Rotation.Elements[0 + 1* 4] = x * y * omc - z * s;
    Rotation.Elements[1 + 1* 4] = y * omc + c;
    Rotation.Elements[2 + 1* 4] = y * z * omc + x * s;

    Rotation.Elements[0 + 2* 4] = x * z * omc + y * s;
    Rotation.Elements[1 + 2* 4] = y * z * omc - x * s;
    Rotation.Elements[2 + 2* 4] = z * omc + c;

    return Rotation;
}

Mat4 Mat4Scale(float x, float y, float z)
{
    Mat4 Scale;
    Mat4Diagonal(&Scale, 1.0f);

    Scale.Elements[0 + 0 * 4] = x;
    Scale.Elements[1 + 1 * 4] = y;
    Scale.Elements[2 + 2 * 4] = z;

    return Scale;
}
