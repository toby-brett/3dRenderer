

#include "MatrixVectorLib.h"
#include <stdio.h>
#include <math.h>

vec3d VectorNormalise(vec3d *v)
{
    float l = VectorLength(v);
    return VectorDivide(v, l);
}

vec3d VectorAdd(vec3d *v1, vec3d *v2)
{
    vec3d result = {
        .x = v1->x + v2->x,
        .y = v1->y + v2->y,
        .z = v1->z + v2->z};

    return result;
}

vec3d VectorMultiply(vec3d *v, float f)
{
    vec3d o = {v->x * f, v->y * f, v->z * f, v->w * f};
    return o;
}

vec3d VectorSubtract(vec3d *v1, vec3d *v2)
{
    vec3d result = {
        .x = v1->x - v2->x,
        .y = v1->y - v2->y,
        .z = v1->z - v2->z};

    return result;
}

vec3d VectorDivide(vec3d *v, float s)
{
    vec3d result = {
        .x = v->x / s,
        .y = v->y / s,
        .z = v->z / s};

    return result;
}

float VectorLength(vec3d *v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

vec3d CrossProduct(vec3d *a, vec3d *b)
{
    vec3d result = {
        .x = a->y * b->z - a->z * b->y,
        .y = a->z * b->x - a->x * b->z,
        .z = a->x * b->y - a->y * b->x};

    return result;
}

vec3d VectorMatrixMult4d(vec3d *i, mat4x4 *m) // MAT * VEC
{
    vec3d o;
    float w = 1.0f;

    o.x = i->x * m->m[0][0] + i->y * m->m[0][1] + i->z * m->m[0][2] + w * m->m[0][3]; // m[0][1] is row 0 col 1
    o.y = i->x * m->m[1][0] + i->y * m->m[1][1] + i->z * m->m[1][2] + w * m->m[1][3];
    o.z = i->x * m->m[2][0] + i->y * m->m[2][1] + i->z * m->m[2][2] + w * m->m[2][3];
    o.w = i->x * m->m[3][0] + i->y * m->m[3][1] + i->z * m->m[3][2] + w * m->m[3][3];

    return o;
}

vec3d Project(vec3d *i, mat4x4 *m)
{
    vec3d o;
    float w = 1.0f;

    o.x = i->x * m->m[0][0] + i->y * m->m[0][1] + i->z * m->m[0][2] + w * m->m[0][3];
    o.y = i->x * m->m[1][0] + i->y * m->m[1][1] + i->z * m->m[1][2] + w * m->m[1][3];
    o.z = i->x * m->m[2][0] + i->y * m->m[2][1] + i->z * m->m[2][2] + w * m->m[2][3];
    float w_ = i->x * m->m[3][0] + i->y * m->m[3][1] + i->z * m->m[3][2] + w * m->m[3][3];

    if (w_ != 0.0f)
    {
        o.x /= w_;
        o.y /= w_;
        o.z /= w_;
    }

    return o;
}

mat4x4 RotationMatrixZ(float theta)
{
    mat4x4 rotZ = {
        .m = {
            {cosf(theta), -sinf(theta), 0.0f, 0.0f},
            {sinf(theta), cosf(theta), 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}}};

    return rotZ;
}

mat4x4 RotationMatrixX(float theta)
{
    mat4x4 rotX = {
        .m = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, cosf(theta), -sinf(theta), 0.0f},
            {0.0f, sinf(theta), cosf(theta), 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}}};

    return rotX;
}

mat4x4 RotationMatrixY(float theta)
{
    mat4x4 rotY = {
        .m = {
            {cosf(theta), 0.0f, sinf(theta), 0.0f},
            {0.0f, 1.0, 0.0, 0.0f},
            {-sinf(theta), 0.0f, cosf(theta), 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}}};

    return rotY;
}

mat4x4 TranslationMatrix(float x, float y, float z)
{
    mat4x4 trans = {
        .m = {
            {1.0f, 0.0f, 0.0f, x},
            {0.0f, 1.0f, 0.0f, y},
            {0.0f, 0.0f, 1.0f, z},
            {0.0f, 0.0f, 0.0f, 1.0f}}};

    return trans;
}

vec3d getNorm(triangle *t)
{
    vec3d N;
    vec3d a;
    vec3d b;

    vec3d o;

    a.x = t->p[1].x - t->p[0].x;
    a.y = t->p[1].y - t->p[0].y;
    a.z = t->p[1].z - t->p[0].z;

    b.x = t->p[2].x - t->p[0].x;
    b.y = t->p[2].y - t->p[0].y;
    b.z = t->p[2].z - t->p[0].z;

    o = CrossProduct(&a, &b);

    o = VectorNormalise(&o);

    return o;
}

mat4x4 initProjMat(float fFov, float fAspectRatio, float fNear, float fFar)
{
    float fFovRad = 1.0f / tanf(fFov * 0.5f / 180.0f * 3.14159f);
    float q = fFar / (fFar - fNear);

    mat4x4 matProj = {.m = {
                          {fAspectRatio * fFovRad, 0.0f, 0.0f, 0.0f}, // Column 0
                          {0.0f, fFovRad, 0.0f, 0.0f},                // Column 1
                          {0.0f, 0.0f, q, -fNear * q},                // Column 2
                          {0.0f, 0.0f, 1.0f, 0.0f}                    // Column 3
                      }};

    return matProj;
}

mat4x4 MatrixMultiply(mat4x4 *m1, mat4x4 *m2)
{
    mat4x4 result;
    for (int x = 0; x < 4; x++)
    {
        for (int y = 0; y < 4; y++)
        {
            result.m[y][x] = m1->m[y][0] * m2->m[0][x] +
                             m1->m[y][1] * m2->m[1][x] +
                             m1->m[y][2] * m2->m[2][x] +
                             m1->m[y][3] * m2->m[3][x];
        }
    }
    return result;
}

mat4x4 MakeIdentity()
{
    mat4x4 mat = {
        .m = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}}};

    return mat;
}

mat4x4 ViewMatrix(vec3d *pos, vec3d *target, vec3d *up)
{
    vec3d forward = VectorSubtract(target, pos);
    forward = VectorNormalise(&forward);

    vec3d a = VectorMultiply(&forward, dotProduct(up, &forward));
    vec3d newUp = VectorSubtract(up, &a);
    newUp = VectorNormalise(&newUp);

    vec3d right = CrossProduct(&newUp, &forward);

    mat4x4 matrix = {.m = {
                         {right.x, right.y, right.z, -dotProduct(&right, pos)},
                         {newUp.x, newUp.y, newUp.z, -dotProduct(&newUp, pos)},
                         {forward.x, forward.y, forward.z, -dotProduct(&forward, pos)},
                         {0.0f, 0.0f, 0.0f, 1.0f}}};
    return matrix;
}

float dotProduct(vec3d *a, vec3d *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void PrintMat(mat4x4 *mat)
{
    for (int i = 0; i < 4; i++)
    {
        printf("[ %f %f %f %f ] \n", mat->m[i][0], mat->m[i][1], mat->m[i][2], mat->m[i][3]);
    }
    printf("\n");
}

void PrintTri(triangle tri)
{
    for (int i = 0; i < 3; i++)
    {
        printf("p%i: (%f %f %f)\n", i, tri.p[i].x, tri.p[i].y, tri.p[i].z);
    }
    printf("\n");
}

void PrintVec(vec3d vec)
{
    printf("%f %f %f \n", vec.x, vec.y, vec.z);
    printf("\n");
}
