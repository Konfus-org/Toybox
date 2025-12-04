#include "OpenGLRenderingPlugin.h"
#include "OpenGLShader.h"
#include "OpenGLMesh.h"
#include "OpenGLTexture.h"
#include <Tbx/Debug/Tracers.h>
#include <glad/glad.h>

namespace Tbx::Plugins::OpenGLRendering
{
    static void GLAPIENTRY GlMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
    {
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
                TBX_ASSERT(false, "GL CALLBACK: type = {}, severity = {}, message = {}\n", type, severity, message);
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                TBX_TRACE_ERROR("GL CALLBACK: type = {}, severity = {}, message = {}\n", type, severity, message);
                break;
            case GL_DEBUG_SEVERITY_LOW:
                TBX_TRACE_WARNING("GL CALLBACK: type = {}, severity = {}, message = {}\n", type, severity, message);
                break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                TBX_TRACE_INFO("GL CALLBACK: type = {}, severity = {}, message = {}\n", type, severity, message);
                break;
            default:
                TBX_TRACE_WARNING("GL CALLBACK: type = {}, severity = {}, message = {}\n", severity, message);
                break;
        }
    }

    GraphicsApi OpenGLRenderingPlugin::GetApi() const
    {
        return GraphicsApi::OpenGL;
    }

    void OpenGLRenderingPlugin::EnableDepthTesting(bool enabled)
    {
        if (enabled) glDepthMask(GL_TRUE);
        else glDepthMask(GL_FALSE);
    }

    void OpenGLRenderingPlugin::SetContext(Ref<IGraphicsContext> context)
    {
        if (!_isGlInitialized)
        {
            InitializeOpenGl();
        }
    }

    void OpenGLRenderingPlugin::BeginDraw(const RgbaColor& clearColor, const Viewport& viewport)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(clearColor.R, clearColor.G, clearColor.B, clearColor.A);
        glViewport((GLint)viewport.Position.X, (GLint)viewport.Position.Y, viewport.Extends.Width, viewport.Extends.Height);
    }

    void OpenGLRenderingPlugin::EndDraw()
    {
        glFlush();
    }

    Ref<TextureResource> OpenGLRenderingPlugin::UploadTexture(const Texture& texture)
    {
        return Ref<TextureResource>(new OpenGLTexture(texture), [this](TextureResource* resource) { DeleteResource(resource); });
    }

    Ref<MeshResource> OpenGLRenderingPlugin::UploadMesh(const Mesh& mesh)
    {
        return Ref<MeshResource>(new OpenGLMesh(mesh), [this](MeshResource* resource) { DeleteResource(resource); });
    }

    Ref<ShaderProgramResource> OpenGLRenderingPlugin::CreateShaderProgram(const std::vector<Ref<ShaderResource>>& shadersToLink)
    {
        return Ref<ShaderProgramResource>(new OpenGLShaderProgram(shadersToLink), [this](ShaderProgramResource* resource) { DeleteResource(resource); });
    }

    Ref<ShaderResource> OpenGLRenderingPlugin::CompileShader(const Shader& shader)
    {
        return Ref<OpenGLShader>(new OpenGLShader(shader), [this](OpenGLShader* resource) { DeleteResource(resource); });
    }

    void OpenGLRenderingPlugin::InitializeOpenGl()
    {
        TBX_TRACE_INFO("GL Rendering: Initializing OpenGl...\n");

        // Print gl info
        TBX_TRACE_INFO("GL Rendering: OpenGL Info:");
        const std::string& vendorVersion = (const char*)glGetString(GL_VENDOR);
        TBX_TRACE_INFO("  Vendor: {0}", vendorVersion);
        const std::string& rendererVersion = (const char*)glGetString(GL_RENDERER);
        TBX_TRACE_INFO("  Renderer: {0}", rendererVersion);
        const std::string& glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        TBX_TRACE_INFO("  GLSL: {0}", glslVersion);
        const std::string& openGLVersion = (const char*)glGetString(GL_VERSION);
        TBX_TRACE_INFO("  Version: {0}\n", openGLVersion);

        // Check OpenGL version
        TBX_ASSERT((GLVersion.major == 4 && GLVersion.minor >= 5), "GL Rendering: requires at least OpenGL version 4.5!");

#ifdef TBX_DEBUG
        // Enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(GlMessageCallback, 0);
#endif

        // Congifure global gl stuff
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        _isGlInitialized = true;
    }

    void OpenGLRenderingPlugin::DeleteResource(GraphicsResource* resourceToDelete)
    {
        delete resourceToDelete;
    }
}