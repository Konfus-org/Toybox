#include "OpenGLRenderer.h"

namespace OpenGLRendering
{

    void OpenGLRenderer::Initialize(const std::weak_ptr<Toybox::IWindow>& context)
    {
        _shader = std::make_unique<OpenGLShader>();
        _context = std::make_unique<OpenGLContext>(context);
        _vertArray = std::make_unique<OpenGLVertexArray>();
        _vertArray->Bind();

        // TODO: pass shader source to renderer instead of hard coding here...
        // Compile and bind our shader
        const auto& vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec4 inColor;

            out vec3 position;
            out vec4 color;
            
            void main()
            {
                position = inPosition;
                color = inColor;
                gl_Position = vec4(inPosition, 1.0);
            }
        )";


        const auto& fragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 outColor;

            in vec3 position;
            in vec4 color;
            
            void main()
            {
                outColor = vec4(position * 0.5 + 0.5, 1.0);
                outColor = color;
            }
        )";

        _shader->Compile(vertexSrc, fragmentSrc);
        _shader->Bind();
    }

    void OpenGLRenderer::Shutdown()
    {
        _vertArray->Clear();
        _vertArray->Unbind();
    }

    void OpenGLRenderer::BeginFrame()
    {
        // Clear screen at the beginning of our frame
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndFrame()
    {
        // Draw at the end of our frame if we have anything to draw
        if (const auto& indexCount = _vertArray->GetIndexCount(); indexCount > 0)
        {
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        }
        _context->SwapBuffers();
    }

    void OpenGLRenderer::ClearScreen()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(const Toybox::Color& color)
    {
        glClearColor(color.R, color.G, color.B, color.A);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(const Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale)
    {
        // Clear old buffer
        _vertArray->Clear();

        // Set vertex buffer
        const auto& meshVertexBuffer = mesh.GetVertexBuffer();
        _vertArray->AddVertexBuffer(meshVertexBuffer);

        // Set index buffer
        const auto& meshIndexBuffer = mesh.GetIndexBuffer();
        _vertArray->SetIndexBuffer(meshIndexBuffer);
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
        _context->SetSwapInterval(enabled);
    }

    std::string OpenGLRenderer::GetRendererName() const
    {
        return "OpenGL";
    }
}
