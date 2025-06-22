#include <emscripten/emscripten.h>
#include <GLES3/gl3.h>
#include <stdlib.h>
#include <stdio.h>
#include <emscripten/html5.h>
#include <math.h>
#include "MatrixVectorLib.h" // my own made class
#include <string.h>

// NOTE: OPENGL USES COLUMN MAJOR MATRIX

const char *vertex_shader_src =
    "attribute vec3 position;"
    "attribute vec4 color;"
    "varying vec4 vColor;"
    "void main() {"
    "  gl_Position = vec4(position, 1.0);"
    "  vColor = color;"
    "}";

const char *fragment_shader_src =
    "precision mediump float;"
    "varying vec4 vColor;"
    "void main() {"
    "  gl_FragColor = vColor;"
    "}";

typedef struct Object
{

    mesh objectMesh;
    GLuint program;
    GLuint vbo;
    GLuint ebo; // element array buffer
    GLuint cbo; // color buffer
    GLint colorLoc;

    float fTheta;
    float zPos;
    float xPos;
    float yPos;
    int sw, sh;

    void (*init)(struct Object *self, char *sFilePath); // pointer named init - points to a function which takes struct Cube* as an arg
    void (*render)(struct Object *self, float time_elapsed, float *vertexBuffer, float *colorBuffer, int *indexBuffer, int *vertexCount, int *colorCount, int *indexCount);

} Object;

mesh CreateMesh(char *sFilename)
{

    int MAX_OBJ_VERTICIES = 6000;
    int MAX_OBJ_FACES = 10000;

    vec3d *v = malloc(sizeof(vec3d) * MAX_OBJ_VERTICIES);
    face *f = malloc(sizeof(vec3d) * MAX_OBJ_FACES);

    int v_index = 0;
    int f_index = 0;

    FILE *file = fopen(sFilename, "r");
    if (!file)
    {
        perror("failed to open .obj file");
    }

    char line[128]; // 128 characters long
    while (fgets(line, sizeof(line), file))
    { // destination buffer, (128 chars), file pointer
        if (strncmp(line, "v ", 2) == 0)
        { // first 2 = "v "
            vec3d vertex;
            sscanf(line, "v %f %f %f", &vertex.x, &vertex.y, &vertex.z); // Parses a line like "v x y z" and stores the three floats into vertex.x, vertex.y, and vertex.z. Uses & to pass variable addresses to sscanf.
            v[v_index++] = vertex;                                       // increases AFTER use
        }
        if (strncmp(line, "f ", 2) == 0)
        { // first 2 = "v "
            face face;
            sscanf(line, "f %i %i %i", &face.p1, &face.p2, &face.p3); // Parses a line like "v x y z" and stores the three floats into vertex.x, vertex.y, and vertex.z. Uses & to pass variable addresses to sscanf.
            face.p1--;
            face.p2--;
            face.p3--; // becuase faces start counting from 1 (STUPID)
            f[f_index++] = face;
        }
    }

    mesh object;
    object.count = f_index; // starts at 0
    object.capactiy = f_index;

    object.tris = malloc(sizeof(triangle) * (f_index)); // how much memory to alloc for trianlges

    if (object.tris == NULL)
    {
        printf("Failed to load mesh!\n");
        object.count = 0;
        object.capactiy = 0;
        return object;
    }

    for (int i = 0; i < object.capactiy; i++)
    { // through faces
        face currentFace = f[i];
        object.tris[i] = (triangle){{v[currentFace.p1], v[currentFace.p2], v[currentFace.p3]}};
    }

    free(v);
    free(f);
    return object;
}

GLuint program;
GLuint vbo;
GLuint vao;

mat4x4 matProj;
GLint colorLoc;
Object object;
double height;
double width;
double lastFrameTime = 0.0;
vec3d vCamera = {0, 0, 0};

#define MAX_VERTS 360000

float globalVertexBuffer[MAX_VERTS];
int globalVertexCount = 0;

float globalColorBuffer[(MAX_VERTS) * 4]; // RBGA per face
int globalColorCount = 0;

int globalIndexBuffer[MAX_VERTS / 3];
int globalIndexCount = 0;

EM_BOOL keydown(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{

    if (strcmp(e->key, "w") == 0)
    {
        object.zPos += 0.2f;
    }
    else if (strcmp(e->key, "a") == 0)
    {
        object.xPos -= 0.2f;
    }
    else if (strcmp(e->key, "s") == 0)
    {
        object.zPos -= 0.2f;
    }
    else if (strcmp(e->key, "d") == 0)
    {
        object.xPos += 0.2f;
    }

    return EM_TRUE;
}

void fetch_proj_mat()
{
    emscripten_get_element_css_size("#canvas", &width, &height);
    emscripten_set_canvas_element_size("#canvas", (int)height, (int)width);
    glViewport(0, 0, (int)height, (int)width);

    float fNear = 0.1f;
    float fFar = 1000.0f;
    float fFov = 90.0f;
    float fAspectRatio = (float)height / (float)width;

    matProj = initProjMat(fFov, fAspectRatio, fNear, fFar);
}

EM_BOOL on_resize(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
{
    fetch_proj_mat();
    return EM_TRUE;
}

void object_init(Object *self, char *sFileName)
{
    self->program = program;
    self->objectMesh = CreateMesh(sFileName);

    self->fTheta = 0.0f;
    self->xPos = 0.0f;
    self->yPos = 0.0f;
    self->zPos = 0.0f;

    glUseProgram(self->program);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // --- Vertex positions
    glGenBuffers(1, &self->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, self->vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    GLint pos = glGetAttribLocation(self->program, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // --- Color buffer
    glGenBuffers(1, &self->cbo);
    glBindBuffer(GL_ARRAY_BUFFER, self->cbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    GLint col = glGetAttribLocation(self->program, "color");
    glEnableVertexAttribArray(col);
    glVertexAttribPointer(col, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // --- Index buffer
    glGenBuffers(1, &self->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_VERTS * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
}

void object_render(Object *self, float time_elapsed, float *vertexBuffer, float *colorBuffer, int *indexBuffer, int *vertexCount, int *colorCount, int *indexCount)
{
    //  transform outside loop - then apply to each trianlge

    mat4x4 matRotX = RotationMatrixX(self->fTheta);
    mat4x4 matRotY = RotationMatrixY(self->fTheta);
    mat4x4 matRotZ = RotationMatrixZ(self->fTheta);

    mat4x4 matTrans = TranslationMatrix(self->xPos, self->yPos, self->zPos);

    mat4x4 identity = MakeIdentity();

    mat4x4 matWorld = MatrixMultiply(&matRotZ, &matRotX);
    matWorld = MatrixMultiply(&matTrans, &matWorld);

    self->fTheta += 1.0f * time_elapsed;

    for (int i = 0; i < self->objectMesh.count; i++) // iterates through TRIANGLES
    {

        GLuint base = (*vertexCount) / 3;

        triangle tri = self->objectMesh.tris[i];

        triangle triTransformed;
        triTransformed.p[0] = VectorMatrixMult4d(&tri.p[0], &matWorld);
        triTransformed.p[1] = VectorMatrixMult4d(&tri.p[1], &matWorld);
        triTransformed.p[2] = VectorMatrixMult4d(&tri.p[2], &matWorld);

        vec3d viewDirection = VectorSubtract(&triTransformed.p[0], &vCamera);
        viewDirection = VectorNormalise(&viewDirection);

        vec3d normal = getNorm(&triTransformed);

        float dot = dotProduct(&viewDirection, &normal);

        if (dot < 0)
        {
            // illumination
            vec3d light_direction = {0.0f, 0.0f, -1.0f};
            //   light_direction = VectorNormalise(&light_direction);

            float lightDot = dotProduct(&light_direction, &normal);
            float color = fmaxf(0.0f, lightDot * 0.8f); // prevent negative color

            // project 2d - 3d

            triangle triProjected;
            triProjected.p[0] = Project(&triTransformed.p[0], &matProj);
            triProjected.p[1] = Project(&triTransformed.p[1], &matProj);
            triProjected.p[2] = Project(&triTransformed.p[2], &matProj);

            //    glUniform4f(self->colorLoc, color, color, color, 1.0f);

            vertexBuffer[(*vertexCount)++] = triProjected.p[0].x;
            vertexBuffer[(*vertexCount)++] = triProjected.p[0].y;
            vertexBuffer[(*vertexCount)++] = triProjected.p[0].z;

            vertexBuffer[(*vertexCount)++] = triProjected.p[1].x;
            vertexBuffer[(*vertexCount)++] = triProjected.p[1].y;
            vertexBuffer[(*vertexCount)++] = triProjected.p[1].z;

            vertexBuffer[(*vertexCount)++] = triProjected.p[2].x;
            vertexBuffer[(*vertexCount)++] = triProjected.p[2].y;
            vertexBuffer[(*vertexCount)++] = triProjected.p[2].z;

            for (int i = 0; i < 3; i++)
            {
                colorBuffer[(*colorCount)++] = 1.0f;
                colorBuffer[(*colorCount)++] = color;
                colorBuffer[(*colorCount)++] = color;
                colorBuffer[(*colorCount)++] = 1.0f;
            }

            // now three line‐segments p0→p1, p1→p2, p2→p0
            indexBuffer[(*indexCount)++] = base + 0;
            indexBuffer[(*indexCount)++] = base + 1;

            indexBuffer[(*indexCount)++] = base + 1;
            indexBuffer[(*indexCount)++] = base + 2;

            indexBuffer[(*indexCount)++] = base + 2;
            indexBuffer[(*indexCount)++] = base + 0;
        }
    }
}

void init()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader_src, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader_src, NULL);
    glCompileShader(fs);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glUseProgram(program);
    glEnable(GL_DEPTH_TEST);
}

void render()
{
    double now = emscripten_get_now();
    float delta = (float)(now - lastFrameTime) / 1000; // seconds
    lastFrameTime = now;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    globalVertexCount = 0;
    globalIndexCount = 0;
    globalColorCount = 0;

    object.render(&object, delta, globalVertexBuffer, globalColorBuffer, globalIndexBuffer, &globalVertexCount, &globalColorCount, &globalIndexCount);

    glUseProgram(program);

    // Rebind VAO and set both attribute pointers again
    glBindVertexArray(vao);

    // Rebind vertex buffer and pointer
    glBindBuffer(GL_ARRAY_BUFFER, object.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, globalVertexCount * sizeof(float), globalVertexBuffer);
    GLint pos = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Rebind color buffer and pointer
    glBindBuffer(GL_ARRAY_BUFFER, object.cbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, globalColorCount * sizeof(float), globalColorBuffer);
    GLint col = glGetAttribLocation(program, "color");
    glEnableVertexAttribArray(col);
    glVertexAttribPointer(col, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Upload index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object.ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, globalIndexCount * sizeof(GLuint), globalIndexBuffer);

    glDrawArrays(GL_TRIANGLES, 0, globalVertexCount / 3);

    float end = emscripten_get_now();

    printf("%f\n", 1.0f / delta);
}

int main()
{
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = EM_FALSE;
    attr.depth = EM_TRUE;
    attr.stencil = EM_FALSE;
    attr.antialias = EM_TRUE;
    attr.majorVersion = 2;
    attr.minorVersion = 0;

    ctx = emscripten_webgl_create_context("#canvas", &attr);
    emscripten_webgl_make_context_current(ctx);

    emscripten_set_canvas_element_size("#canvas", 800, 600);

    glClearColor(1.0, 1.0, 1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    init();

    object.init = object_init;
    object.render = object_render;
    object.init(&object, "/assets/teapot.obj");

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_FALSE, on_resize);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, keydown);

    fetch_proj_mat();

    emscripten_set_main_loop(render, 0, 1);

    return 0;
}
