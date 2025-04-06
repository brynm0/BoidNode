#ifndef GL_RENDER_H
#define GL_RENDER_H

#ifdef __cplusplus
extern "C"
{
#endif

    // -----------------------------------------------------------------------------
    // Public API: Structures and function prototypes
    // -----------------------------------------------------------------------------

    // Initializes the OpenGL renderer on a Win32 window (HWND) with the given width and height.
    // Returns 0 on success; nonzero on failure.
    int gl_render_init(void *windowHandle, int width, int height);

    // Pass a mesh (stored on the CPU) to OpenGL for rendering. This will create and upload
    // the vertex (and optionally, index) buffers.
    void gl_render_set_mesh(const Mesh *mesh);

    // Pass a Model-View-Projection (MVP) matrix to the OpenGL code. The matrix is expected to be
    // a 4x4 float array in column-major order (as GLSL expects).
    void gl_render_set_mvp(const float mvp[16]);

    // Performs a single render pass: clears the screen, draws the current mesh using the current MVP.
    void gl_render_draw();

    // Releases all OpenGL resources created by gl_render_init() and related functions.
    void gl_render_cleanup(void);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// Implementation (define GL_RENDER_IMPLEMENTATION in one source file to include it)
// -----------------------------------------------------------------------------
// #ifdef GL_RENDER_IMPLEMENTATION

// ---------- Standard headers and OpenGL/GLEW headers ----------
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLEW_STATIC
#include "GL/glew.h"
#include "GL/wglew.h"
#include "GL/GL.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

#include "math_linear.h"

// Modify the UniformBuffer structure to match the shader
struct ubo_matrix
{
    mat4 mvp;
    mat4 model;
    mat4 view;
    vec4 view_pos;
};

// Add these new structures for lighting
typedef struct
{
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} ubo_light;

typedef struct
{
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
    float padding[3]; // Padding to ensure 16-byte alignment
} ubo_material;

// ---------- Internal global variables for window and context ----------
static HWND gl_hWnd = 0;
static HDC g_hDC = 0;
static HGLRC g_hRC = 0;
static int g_width = 800;
static int g_height = 600;

struct gl_mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;         // Element Buffer Object (if using indices)
    GLuint matrix_ubo = 0;  // Uniform buffer object for matrices
    GLint mvpLocation = -1; // Uniform location for the MVP matrix
    mat4 model_matrix = {};

    unsigned int meshVertexCount = 0;
    unsigned int meshIndexCount = 0;
};

std::vector<gl_mesh> g_meshes; // Store multiple meshes
GLuint g_shaderProgram = 0;
// ---------- Global variables for OpenGL objects ----------
// static GLuint g_shaderProgram = 0;
// static GLuint g_VAO = 0;
// static GLuint g_VBO = 0;
// static GLuint g_EBO = 0;         // Element Buffer Object (if using indices)
// static GLint g_mvpLocation = -1; // Uniform location for the MVP matrix
// static unsigned int g_meshVertexCount = 0;
// static unsigned int g_meshIndexCount = 0;

// ---------- Utility function for error handling ----------
static void gl_check_error(const char *msg)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        fprintf(stderr, "OpenGL error: %s (code 0x%x)\n", msg, err);
        // In production you might want to handle errors more gracefully.
        exit(-1);
    }
}

// ---------- Internal helper functions ----------

// Compiles a shader of the given type from the provided source string.
static GLuint CompileShader(GLenum shaderType, const char *source)
{
    GLuint shader = glCreateShader(shaderType);
    if (!shader)
    {
        fprintf(stderr, "Failed to create shader.\n");
        exit(-1);
    }
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char *)malloc(logLength);
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        free(log);
        exit(-1);
    }
    return shader;
}

// Links a vertex and fragment shader into a shader program.
static GLuint CreateShaderProgram()
{
    // Vertex shader: accepts 3-component positions and applies an MVP transformation.
    uint32_t vertex_shader_len_bytes = 0;
    char *vertex_shader_source = (char *)read_file("shaders\\basic_vertex.vert", &vertex_shader_len_bytes);
    uint32_t fragment_shader_len_bytes = 0;
    char *fragment_shader_source = (char *)read_file("shaders\\basic_fragment.frag", &fragment_shader_len_bytes);

    GLuint vertShader = CompileShader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragShader = CompileShader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = glCreateProgram();
    if (!program)
    {
        fprintf(stderr, "Failed to create shader program.\n");
        exit(-1);
    }
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char *)malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        fprintf(stderr, "Shader linking error: %s\n", log);
        free(log);
        exit(-1);
    }
    // Shaders can be detached and deleted after linking.
    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return program;
}

// Function pointer for wglChoosePixelFormatARB
PFNWGLCHOOSEPIXELFORMATARBPROC win_choose_pixel_format_arb = nullptr;

// Dummy window class to get function pointers
void CreateTemporaryContext()
{

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const char *tempClassName = "TempGLWindowClass";

    // Register temporary window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInstance;
    wc.lpszClassName = tempClassName;
    if (!RegisterClassA(&wc))
    {
        fprintf(stderr, "Failed to register temporary window class.\n");
        exit(-1);
    }

    // Create a temporary hidden window
    HWND tempHwnd = CreateWindowA(tempClassName, "Temp", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr);
    if (!tempHwnd)
    {
        fprintf(stderr, "Failed to create temporary window.\n");
        exit(-1);
    }

    HDC tempDC = GetDC(tempHwnd);
    if (!tempDC)
    {
        fprintf(stderr, "Failed to get temporary device context.\n");
        exit(-1);
    }

    // Set a simple pixel format for the temporary context
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(tempDC, &pfd);
    if (!SetPixelFormat(tempDC, pixelFormat, &pfd))
    {
        fprintf(stderr, "Failed to set temporary pixel format.\n");
        exit(-1);
    }

    // Create temporary OpenGL context
    HGLRC tempContext = wglCreateContext(tempDC);
    if (!tempContext || !wglMakeCurrent(tempDC, tempContext))
    {
        fprintf(stderr, "Failed to initialize temporary OpenGL context.\n");
        exit(-1);
    }

    // Load wglChoosePixelFormatARB
    win_choose_pixel_format_arb = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

    // Cleanup temporary resources
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempContext);
    ReleaseDC(tempHwnd, tempDC);
    DestroyWindow(tempHwnd);
    UnregisterClassA(tempClassName, hInstance);
}

int ChooseMultisamplePixelFormat(HDC hdc, int samples)
{
    int pixelFormat = 0;
    UINT numFormats = 0;

    int attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 1, // Enable multisampling
        WGL_SAMPLES_ARB, samples,  // Number of samples
        0                          // End of attributes
    };

    if (win_choose_pixel_format_arb)
    {
        if (win_choose_pixel_format_arb(hdc, attribs, nullptr, 1, &pixelFormat, &numFormats))
        {
            if (numFormats > 0)
            {
                return pixelFormat;
            }
        }
    }
    return 0; // Fallback if multisampling is not available
}

static void SetupOpenGLContext()
{
    CreateTemporaryContext();

    g_hDC = GetDC(gl_hWnd);
    if (!g_hDC)
    {
        fprintf(stderr, "Failed to get device context.\n");
        exit(-1);
    }
    // PIXELFORMATDESCRIPTOR pfd = {
    //     sizeof(PIXELFORMATDESCRIPTOR),
    //     1,
    //     PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    //     PFD_TYPE_RGBA,
    //     32,               // Color bits
    //     0, 0, 0, 0, 0, 0, // Color bits shift, etc.
    //     0, 0,             // No alpha buffer
    //     0, 0, 0, 0, 0,    // Accumulation buffer
    //     24,               // Depth bits (added)
    //     8,                // Stencil bits (optional)
    //     0,                // No auxiliary buffers
    //     PFD_MAIN_PLANE,
    //     0, 0, 0, 0};

    // PIXELFORMATDESCRIPTOR pfd;
    // memset(&pfd, 0, sizeof(pfd));
    // pfd.nSize = sizeof(pfd);
    // pfd.nVersion = 1;
    // pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    // pfd.iPixelType = PFD_TYPE_RGBA;
    // pfd.cColorBits = 32;
    // pfd.cDepthBits = 24;
    // pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChooseMultisamplePixelFormat(g_hDC, 4); // Try to get a multisample pixel format

    PIXELFORMATDESCRIPTOR pfd = {};
    DescribePixelFormat(g_hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
    if (!SetPixelFormat(g_hDC, pf, &pfd))
    {
        fprintf(stderr, "Failed to set pixel format.\n");
        DWORD error = GetLastError();
        fprintf(stderr, "Error code: 0x%x\n", error);
        exit(-1);
    }

    // Create a temporary context
    HGLRC tempContext = wglCreateContext(g_hDC);
    if (!tempContext)
    {
        fprintf(stderr, "Failed to create temporary OpenGL context.\n");
        exit(-1);
    }
    if (!wglMakeCurrent(g_hDC, tempContext))
    {
        fprintf(stderr, "Failed to make temporary OpenGL context current.\n");
        exit(-1);
    }

    // Initialize GLEW to load OpenGL extensions
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
        exit(-1);
    }

    // Load wglCreateContextAttribsARB
    if (!wglCreateContextAttribsARB)
    {
        fprintf(stderr, "wglCreateContextAttribsARB not supported.\n");
        exit(-1);
    }

    // Create the actual OpenGL context with attributes
    GLint attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0};

    g_hRC = wglCreateContextAttribsARB(g_hDC, 0, attribs);
    if (!g_hRC)
    {
        fprintf(stderr, "Failed to create OpenGL rendering context.\n");
        exit(-1);
    }

    // Make the new context current and delete the temporary context
    if (!wglMakeCurrent(g_hDC, g_hRC))
    {
        fprintf(stderr, "Failed to make OpenGL context current.\n");
        exit(-1);
    }
    wglDeleteContext(tempContext);

    gl_check_error("After GLEW init");
}

// Add new global variables for lighting
static ubo_light g_currentLight = {
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.2f, 0.0f, 0.2f, 1.0f},
    {0.8f, 0.3f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f}};

static ubo_material g_currentMaterial = {
    {0.1f, 0.1f, 0.1f, 1.0f},
    {0.8f, 0.8f, 0.8f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    128.0f};

// static GLuint g_matrix_ubo = 0;
static GLuint g_materialUBO = 0;
static GLuint g_lightUBO = 0;

static void set_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 position)
{
    g_currentLight.position = {position.x, position.y, position.z, 1.0f};
    g_currentLight.ambient = {ambient.x, ambient.y, ambient.z, 1.0f};
    g_currentLight.diffuse = {diffuse.x, diffuse.y, diffuse.z, 1.0f};
    g_currentLight.specular = {specular.x, specular.y, specular.z, 1.0f};
}

// Update SetupGLObjects to handle the larger UBO
static void SetupGLObjects()
{
    g_shaderProgram = CreateShaderProgram();
    gl_check_error("After shader program creation");

    // Create and bind the uniform buffer
    glGenBuffers(1, &g_materialUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, g_materialUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_material), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Create and bind the uniform buffer
    glGenBuffers(1, &g_lightUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, g_lightUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_light), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind the uniform block
    GLuint blockIndex = glGetUniformBlockIndex(g_shaderProgram, "UniformBuffer");
    glUniformBlockBinding(g_shaderProgram, blockIndex, 0);
}

// -----------------------------------------------------------------------------
// Public API implementations
// -----------------------------------------------------------------------------

// gl_render_init: Initializes the OpenGL renderer by creating an OpenGL context on the
// provided Win32 window and setting up default shader and VAO objects.
int gl_render_init(void *windowHandle, int width, int height)
{
    gl_hWnd = (HWND)windowHandle;
    g_width = width;
    g_height = height;

    // Set up the OpenGL context using WGL.
    SetupOpenGLContext();

    // Set up our shader program, uniform location, and VAO.
    SetupGLObjects();

    // Set default OpenGL state.
    glViewport(0, 0, g_width, g_height);
    glClearColor(0.5f, 0.4f, 0.3f, 1.0f);

    glEnable(GL_FRAMEBUFFER_SRGB); // Enable sRGB framebuffer for color correction

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // Standard depth comparison function

    // Optional: Enable backface culling for better performance
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    gl_check_error("After OpenGL initialization");

    return 0;
}

gl_mesh *gl_render_add_mesh(const Mesh *mesh)
{
    gl_mesh render_mesh = {};

    if (!mesh || !mesh->vertices || mesh->vertexCount == 0)
    {
        fprintf(stderr, "Invalid mesh data.\n");
        return nullptr;
    }
    render_mesh.meshVertexCount = mesh->vertexCount;
    render_mesh.meshIndexCount = mesh->indexCount;
    render_mesh.model_matrix = mat4_identity(); // Initialize model matrix to identity

    // Create and bind the uniform buffer
    glGenBuffers(1, &render_mesh.matrix_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, render_mesh.matrix_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(struct ubo_matrix), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Delete old buffers if they exist
    if (render_mesh.VBO)
        glDeleteBuffers(1, &render_mesh.VBO);
    if (render_mesh.EBO)
        glDeleteBuffers(1, &render_mesh.EBO);

    // Create and fill VBO
    glGenBuffers(1, &render_mesh.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, render_mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertexCount * sizeof(vertex), mesh->vertices, GL_STATIC_DRAW);

    // Create EBO if needed
    if (mesh->indices && mesh->indexCount > 0)
    {
        glGenBuffers(1, &render_mesh.EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->indexCount,
                     mesh->indices, GL_STATIC_DRAW);
    }

    // Create VAO
    glGenVertexArrays(1, &render_mesh.VAO);
    gl_check_error("After VAO creation");
    // Configure VAO
    glBindVertexArray(render_mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, render_mesh.VBO);

    // Use proper stride and offsets
    const GLsizei stride = sizeof(vertex);
    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
    // Normal attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(sizeof(vec4)));
    // TexCoord attribute (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void *)(sizeof(vec4) * 2));

    if (render_mesh.EBO)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render_mesh.EBO);
    }

    glBindVertexArray(0);
    gl_check_error("After setting mesh");
    g_meshes.push_back(render_mesh); // Store the mesh in the global vector
    return &g_meshes[g_meshes.size() - 1];
}
// Update gl_render_set_mvp to handle the full UBO
void gl_render_set_mvp(const mat4 view, const mat4 projection, const camera cam)
{
    mat4 vp = mat4_mult(projection, view);
    for (int i = 0; i < g_meshes.size(); i++)
    {

        ubo_matrix temp = {};
        mat4 mvp = mat4_mult(g_meshes[i].model_matrix, vp);
        gl_mesh *mesh = &g_meshes[i];
        memcpy(temp.mvp.m, mvp.m, sizeof(mat4)); // 16 * sizeof(float));
        memcpy(temp.view.m, view.m, sizeof(mat4));
        memcpy(temp.model.m, mesh->model_matrix.m, sizeof(mat4));

        memcpy(&temp.view_pos, &cam.position, sizeof(vec3));
        temp.view_pos.w = 1.0f; // Set w component to 1.0f

        glBindBuffer(GL_UNIFORM_BUFFER, mesh->matrix_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(temp), &temp);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}

// Add new functions for lighting control
void gl_render_set_light(const ubo_light *light)
{
    if (light)
    {
        memcpy(&g_currentLight, light, sizeof(ubo_light));
    }
}

void gl_render_set_material(const ubo_material *material)
{
    if (material)
    {
        memcpy(&g_currentMaterial, material, sizeof(ubo_material));
    }
}

// Update gl_render_draw to set lighting uniforms
void gl_render_draw(u32 width, u32 height)
{
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glEnable(GL_DEPTH_TEST);

    glUseProgram(g_shaderProgram);

    for (int i = 0; i < g_meshes.size(); i++)
    {

        gl_mesh *mesh = &g_meshes[i];

        // Bind the UBO
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, mesh->matrix_ubo);

        // Bind Material UBO
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, g_materialUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, g_materialUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ubo_material), &g_currentMaterial);
        glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind for safety

        // Bind Light UBO
        glBindBuffer(GL_UNIFORM_BUFFER, g_lightUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ubo_light), &g_currentLight);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, g_lightUBO);

        glBindVertexArray(mesh->VAO);
        if (mesh->EBO && mesh->meshIndexCount > 0)
        {
            glDrawElements(GL_TRIANGLES, mesh->meshIndexCount, GL_UNSIGNED_INT, 0);
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, mesh->meshVertexCount);
        }
        glBindVertexArray(0);
    }

    glUseProgram(0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SwapBuffers(g_hDC);
}

// gl_render_cleanup: Deletes all OpenGL objects and destroys the OpenGL context.
void gl_render_cleanup(void)
{
    for (int i = 0; i < g_meshes.size(); i++)
    {
        gl_mesh *mesh = &g_meshes[i];
        if (mesh->EBO)
        {
            glDeleteBuffers(1, &mesh->EBO);
            mesh->EBO = 0;
        }
        if (mesh->VBO)
        {
            glDeleteBuffers(1, &mesh->VBO);
            mesh->VBO = 0;
        }
        if (mesh->VAO)
        {
            glDeleteVertexArrays(1, &mesh->VAO);
            mesh->VAO = 0;
        }
    }
    if (g_shaderProgram)
    {
        glDeleteProgram(g_shaderProgram);
        g_shaderProgram = 0;
    }
    // Delete the OpenGL context and release the device context.
    if (g_hRC)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_hRC);
        g_hRC = 0;
    }
    if (g_hDC && gl_hWnd)
    {
        ReleaseDC(gl_hWnd, g_hDC);
        g_hDC = 0;
    }
}

// #endif // GL_RENDER_IMPLEMENTATION
#endif // GL_RENDER_H
