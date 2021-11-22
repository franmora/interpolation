#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <gbm.h>

#include <vector>
#include <string>

#include "glad/glad.h"
#include "glad/glad_egl.h"

using namespace std;

static const int Width = 3;
static const int Height = 3;

static const std::string sVertex = R"delim(
#version 310 es

in vec2 TextureCoord;
in vec2 ClipSpaceCoord;

out vec2 UV;

void main()
{
    gl_Position = vec4(ClipSpaceCoord, 0, 1);
    UV = TextureCoord;
};
)delim";

static const std::string sFragment = R"delim(
#version 310 es
precision highp float;

in vec2 UV;

uniform sampler2D Texture;

out vec4 fragColor;

void main()
{
	//vec2 texCoord = UV + vec2(1.0 / 4194304.0, 1.0 / 4194304.0);
	vec2 texCoord = UV + vec2(0, 0);
	fragColor = texture2D(Texture, texCoord);
};
)delim";

template <typename T>
struct vec2
{
    union
    {
        T x;
        T r;
        T s;
    };
    union
    {
        T y;
        T g;
        T t;
    };
    vec2() : x(0.0), y(0.0) {}
    vec2(T _x, T _y) : x(_x), y(_y) {}
};

template <typename T>
struct vec4
{
    union
    {
        T x;
        T r;
        T s;
    };
    union
    {
        T y;
        T g;
        T t;
    };
    union
    {
        T z;
        T b;
        T p;
    };
    union
    {
        T w;
        T a;
        T q;
    };
    vec4() : x(0.0), y(0.0), z(0.0), w(0.0) {}
    vec4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) {}
    bool operator==(const vec4& rhs) const
    {
        if (r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
};

static GLint CompileShader(const GLuint shaderID, const string& shaderCode)
{
    GLint Result = GL_FALSE;

    // Compile Shader
    char const* ShaderSourcePointer = shaderCode.c_str();
    glShaderSource(shaderID, 1, &ShaderSourcePointer, NULL);
    glCompileShader(shaderID);

    // Check Shader
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &Result);
    if (Result == GL_FALSE) {
        int InfoLogLength;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        vector<char> ShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(shaderID, InfoLogLength, NULL, &ShaderErrorMessage[0]);
        printf("%s\n", &ShaderErrorMessage[0]);
    }
    return Result;
}

static GLuint CreateAndLinkProgram(const vector<GLuint> shaderIDs)
{
    GLuint ProgramID = glCreateProgram();
    for (auto& sID : shaderIDs)
    {
        glAttachShader(ProgramID, sID);
    }

    glLinkProgram(ProgramID);

    // Check the program
    GLint Result = GL_FALSE;
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    if (Result == GL_FALSE) {
        int InfoLogLength;
        glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        vector<char> ProgramErrorMessage(InfoLogLength + 1);
        glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
        printf("%s\n", &ProgramErrorMessage[0]);
    }

    for (auto& sID : shaderIDs)
    {
        glDetachShader(ProgramID, sID);
        glDeleteShader(sID);
    }

    return ProgramID;
}

static GLuint LoadShaders(const string& sVertex, const string& sFragment)
{
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    CompileShader(VertexShaderID, sVertex);
    CompileShader(FragmentShaderID, sFragment);
    vector<GLuint> shaderIDs = { VertexShaderID, FragmentShaderID };
    auto ProgramID = CreateAndLinkProgram(shaderIDs);
    return ProgramID;
}

int MatchConfig2Visual(EGLDisplay egl_display, EGLint visual_id, EGLConfig* configs, int count) {

    EGLint id;
    for (int i = 0; i < count; ++i) {
        if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id)) continue;
        if (id == visual_id) return i;
    }
    return -1;
}

int main()
{
    const vector<EGLint> EglConfigAttributes(
        {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 0,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_NONE
        }
    );
    const vector<EGLint> EglContextAttributes(
        {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        }
    );
    const vector<float> SourceGrid({ 0, 0, 1, 0, 0, 1, 1, 1 });
    const vector<float> TargetGrid({ -1, -1, 1, -1, -1, 1, 1, 1 });
    const vector<GLushort> indexBuffer({ 0, 1, 2, 3, 1, 2 });
    const vector<vec4<unsigned char>> sourceImage(
        {
            { 0, 0, 0, 255 }, { 0, 0, 0, 255 },       { 0, 0, 0, 255 },
            { 0, 0, 0, 255 }, { 255, 255, 255, 255 }, { 0, 0, 0, 255 },
            { 0, 0, 0, 255 }, { 0, 0, 0, 255 },       { 0, 0, 0, 255 }
        }
    );
    vector<vec4<unsigned char>> targetImage(Width * Height);

    int FileDesc = open("/dev/dri/by-path/platform-gpu-card", O_RDWR);
    struct gbm_device* GbmDevice = gbm_create_device(FileDesc);
    EGLDisplay eglDisplay = eglGetDisplay(GbmDevice);
    eglInitialize(eglDisplay, NULL, NULL);
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint count = 0;
    eglGetConfigs(eglDisplay, NULL, 0, &count);
    vector<EGLConfig> configs(count, nullptr);

    EGLint numConfigs;
    eglChooseConfig(eglDisplay, EglConfigAttributes.data(), configs.data(), count, &numConfigs);
    int configIndex = MatchConfig2Visual(eglDisplay, GBM_FORMAT_XRGB8888, configs.data(), numConfigs);
    EGLContext EglContext = eglCreateContext(eglDisplay, configs[configIndex], EGL_NO_CONTEXT, EglContextAttributes.data());

    struct gbm_surface* GbmSurface = gbm_surface_create(GbmDevice, 0, 0, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    EGLSurface EglSurface = eglCreateWindowSurface(eglDisplay, configs[configIndex], GbmSurface, NULL);
    eglMakeCurrent(eglDisplay, EglSurface, EglSurface, EglContext);

    gladLoadEGLLoader((GLADloadproc)eglGetProcAddress);
    gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress);

    GLuint Program = LoadShaders(sVertex, sFragment);
    GLuint LocTextureCoord = glGetAttribLocation(Program, "TextureCoord");
    GLuint LocClipSpaceCoord = glGetAttribLocation(Program, "ClipSpaceCoord");
    glUseProgram(Program);

    GLuint SourceGridBuffer;
    glGenBuffers(1, &SourceGridBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, SourceGridBuffer);
    glBufferData(GL_ARRAY_BUFFER, SourceGrid.size() * sizeof(float), SourceGrid.data(), GL_STATIC_DRAW);

    GLuint TargetGridBuffer;
    glGenBuffers(1, &TargetGridBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, TargetGridBuffer);
    glBufferData(GL_ARRAY_BUFFER, TargetGrid.size() * sizeof(float), TargetGrid.data(), GL_STATIC_DRAW);

    glViewport(0, 0, Width, Height);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint TargetTexture;
    glGenTextures(1, &TargetTexture);
    glBindTexture(GL_TEXTURE_2D, TargetTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint Fbo;
    glGenFramebuffers(1, &Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TargetTexture, 0);

    GLuint Vao;
    glGenVertexArrays(1, &Vao);
    glBindVertexArray(Vao);

    GLuint IndexVertices; // Index buffer 
    glGenBuffers(1, &IndexVertices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVertices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indexBuffer.size(), indexBuffer.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, SourceGridBuffer);
    glVertexAttribPointer(LocTextureCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, TargetGridBuffer);
    glVertexAttribPointer(LocClipSpaceCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(LocTextureCoord);
    glEnableVertexAttribArray(LocClipSpaceCoord);

    GLuint SourceTexture;
    glGenTextures(1, &SourceTexture);
    glBindTexture(GL_TEXTURE_2D, SourceTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, sourceImage.data());

    glDrawElements(GL_TRIANGLES, indexBuffer.size(), GL_UNSIGNED_SHORT, nullptr);

    glBindTexture(GL_TEXTURE_2D, TargetTexture);
    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, targetImage.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    printf("One-to-one mapping of a %dx%d texture using...\n");
    printf("......nearest neighbour. Result is %s\n", sourceImage == targetImage ? "EQUAL" : "DIFFERENT");

    glBindTexture(GL_TEXTURE_2D, SourceTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glDrawElements(GL_TRIANGLES, indexBuffer.size(), GL_UNSIGNED_SHORT, nullptr);

    glBindTexture(GL_TEXTURE_2D, TargetTexture);
    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, targetImage.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    printf("...linear interpolation. Result is %s\n", sourceImage == targetImage ? "EQUAL" : "DIFFERENT");

    glDisableVertexAttribArray(LocTextureCoord);
    glDisableVertexAttribArray(LocClipSpaceCoord);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDeleteVertexArrays(1, &Vao);
    glDeleteFramebuffers(1, &Fbo);
    glDeleteBuffers(1, &IndexVertices);
    glDeleteTextures(1, &SourceTexture);
    glDeleteTextures(1, &TargetTexture);
    glDeleteBuffers(1, &SourceGridBuffer);
    glDeleteBuffers(1, &TargetGridBuffer);

    eglDestroySurface(eglDisplay, EglSurface);
    gbm_surface_destroy(GbmSurface);
    eglDestroyContext(eglDisplay, EglContext);
    eglTerminate(eglDisplay);
    gbm_device_destroy(GbmDevice);
    close(FileDesc);

    return EXIT_SUCCESS;
}
