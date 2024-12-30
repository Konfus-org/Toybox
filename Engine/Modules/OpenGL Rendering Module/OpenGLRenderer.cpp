#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::Initialize(const std::weak_ptr<Toybox::IWindow>& context)
    {
        auto buffer = std::make_shared<OpenGLBuffer>(context);
        _buffer = buffer;
        _context = context;

        // TODO: move this out to a seperate shader class
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Basic OpenGL setup
        glGenVertexArrays(1, &_vao);
        glBindVertexArray(_vao);

        // Simple shader for rendering
        const std::string vertexSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aTexCoord;

        uniform mat4 uTransform;

        out vec2 TexCoord;

        void main()
        {
            gl_Position = uTransform * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        })";

        const std::string fragmentSrc = R"(
        #version 330 core
        in vec2 TexCoord;

        uniform vec4 uColor;
        uniform sampler2D uTexture;

        out vec4 FragColor;

        void main()
        {
            FragColor = texture(uTexture, TexCoord) * uColor;
        })";

        _shaderProgram = LoadShader(vertexSrc, fragmentSrc);
        TBX_ASSERT(_shaderProgram, "Failed to load shader program!");
    }

    void OpenGLRenderer::Shutdown()
    {
        glDeleteProgram(_shaderProgram);
        glDeleteVertexArrays(1, &_vao);
    }

    void OpenGLRenderer::BeginFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndFrame()
    {
        _buffer->Swap();
    }

    void OpenGLRenderer::ClearScreen()
    {
        glUseProgram(_shaderProgram);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Color color)
    {
        glUseProgram(_shaderProgram);
        glClearColor(color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    {
        // TODO: Draw mesh
    }

    void OpenGLRenderer::Draw(const Toybox::Texture& texture, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    {
        // TODO: Draw texture
    }

    void OpenGLRenderer::Draw(const std::string& text, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    {
        // TODO: Draw text
    }

    void OpenGLRenderer::SetViewport(const Toybox::Vector2I& screenPos, const Toybox::Size& size)
    {
        glViewport(screenPos.X, screenPos.Y, size.Width, size.Height);
    }

    void OpenGLRenderer::SetVSyncEnabled(const bool& enabled)
    {
        _buffer->SetSwapInterval(enabled);
    }

    std::string OpenGLRenderer::GetRendererName() const
    {
        return "OpenGL";
    }

    GLuint OpenGLRenderer::LoadShader(const std::string& vertexSrc, const std::string& fragmentSrc) const
    {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const char* vertexCode = vertexSrc.c_str();
        glShaderSource(vertexShader, 1, &vertexCode, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fragmentCode = fragmentSrc.c_str();
        glShaderSource(fragmentShader, 1, &fragmentCode, nullptr);
        glCompileShader(fragmentShader);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }
}
