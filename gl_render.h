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

// ---------- Internal global variables for window and context ----------
static HWND gl_hWnd = 0;
static HDC g_hDC = 0;
static HGLRC g_hRC = 0;
static int g_width = 800;
static int g_height = 600;

// ---------- Global variables for OpenGL objects ----------
static GLuint g_shaderProgram = 0;
static GLuint g_VAO = 0;
static GLuint g_VBO = 0;
static GLuint g_EBO = 0;         // Element Buffer Object (if using indices)
static GLint g_mvpLocation = -1; // Uniform location for the MVP matrix
static unsigned int g_meshVertexCount = 0;
static unsigned int g_meshIndexCount = 0;

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

static void SetupOpenGLContext()
{
    g_hDC = GetDC(gl_hWnd);
    if (!g_hDC)
    {
        fprintf(stderr, "Failed to get device context.\n");
        exit(-1);
    }

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_hDC, &pfd);
    if (pf == 0)
    {
        fprintf(stderr, "Failed to choose pixel format.\n");
        exit(-1);
    }
    if (!SetPixelFormat(g_hDC, pf, &pfd))
    {
        fprintf(stderr, "Failed to set pixel format.\n");
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

GLuint mvp_ubo;
// Sets up the VAO and shader program used for drawing.
static void SetupGLObjects()
{
    // Create and compile our shader program.
    g_shaderProgram = CreateShaderProgram();
    gl_check_error("After shader program creation");

    glGenBuffers(1, &mvp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, mvp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Get the uniform block index
    GLuint blockIndex = glGetUniformBlockIndex(g_shaderProgram, "UniformBuffer");

    // Ensure the block is bound to binding point 0
    glUniformBlockBinding(g_shaderProgram, blockIndex, 0);

    // Create a Vertex Array Object to store vertex attribute configuration.
    glGenVertexArrays(1, &g_VAO);
    glBindVertexArray(g_VAO);
    // We do not create a VBO here since it will be created when the mesh is set.
    glBindVertexArray(0);
    gl_check_error("After VAO creation");
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    return 0;
}

// gl_render_set_mesh: Uploads the provided mesh data to GPU memory by creating VBO (and EBO if needed)
// and configuring the VAO for drawing.
void gl_render_set_mesh(const Mesh *mesh)
{
    if (!mesh || !mesh->vertices || mesh->vertexCount == 0)
    {
        fprintf(stderr, "Invalid mesh data.\n");
        return;
    }
    g_meshVertexCount = mesh->vertexCount;
    g_meshIndexCount = mesh->indexCount; // 0 if not provided

    // If a previous VBO exists, delete it.
    if (g_VBO)
    {
        glDeleteBuffers(1, &g_VBO);
        g_VBO = 0;
    }
    glGenBuffers(1, &g_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * mesh->vertexCount,
                 mesh->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // If index data is provided, create an Element Buffer Object.
    if (mesh->indices && mesh->indexCount > 0)
    {
        if (g_EBO)
        {
            glDeleteBuffers(1, &g_EBO);
            g_EBO = 0;
        }
        glGenBuffers(1, &g_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->indexCount,
                     mesh->indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // Configure the VAO: bind VBO, set vertex attribute pointers.
    glBindVertexArray(g_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    // Enable and set the layout for position attribute at location 0.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void *)0);
    // If using an EBO, bind it to the VAO.
    if (g_EBO)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO);
    }
    glBindVertexArray(0);
    gl_check_error("After setting mesh");
}

// gl_render_set_mvp: Updates the shader uniform for the Model-View-Projection matrix.
void gl_render_set_mvp(mat4 mvp)
{

    glBindBuffer(GL_UNIFORM_BUFFER, mvp_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float) * 16, mvp.m);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// gl_render_draw: Clears the screen, binds the shader program and VAO, issues the draw call,
// and swaps the front and back buffers.
void gl_render_draw(void)
{
    glViewport(0, 0, g_width, g_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_shaderProgram);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mvp_ubo);

    glBindVertexArray(g_VAO);
    // Draw using indices if available; otherwise, use the vertex count.
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Wireframe mode for debugging

    if (g_EBO && g_meshIndexCount > 0)
    {
        glDrawElements(GL_TRIANGLES, g_meshIndexCount, GL_UNSIGNED_INT, 0);
    }
    else
    {

        glDrawArrays(GL_TRIANGLES, 0, g_meshVertexCount);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    gl_check_error("After drawing");

    // Swap the buffers to display the rendered image.
    SwapBuffers(g_hDC);
}

// gl_render_cleanup: Deletes all OpenGL objects and destroys the OpenGL context.
void gl_render_cleanup(void)
{
    if (g_EBO)
    {
        glDeleteBuffers(1, &g_EBO);
        g_EBO = 0;
    }
    if (g_VBO)
    {
        glDeleteBuffers(1, &g_VBO);
        g_VBO = 0;
    }
    if (g_VAO)
    {
        glDeleteVertexArrays(1, &g_VAO);
        g_VAO = 0;
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
