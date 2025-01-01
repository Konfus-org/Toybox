#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::Initialize(const std::weak_ptr<Toybox::IWindow>& context)
    {
        _context = std::make_unique<OpenGLContext>(context);
        _vertexBuffer = std::make_unique<OpenGLVertexBuffer>();
        _indexBuffer = std::make_unique<OpenGLIndexBuffer>();

        // Create our vertex array
        glGenVertexArrays(1, &_vertexArray);
        glBindVertexArray(_vertexArray);

        // Bind vertex and index buffers
        _vertexBuffer->Bind();
        _indexBuffer->Bind();

        const Toybox::BufferLayout& bufferLayout = {
            { Toybox::ShaderDataType::Float3, "outPosition" },
            //{ Toybox::ShaderDataType::Float4, "outColor" }
        };

        _vertexBuffer->SetLayout(bufferLayout);

        ////// Tell gpu the layout of our vertex buffer so it can draw it properly
        //_vertexBuffer->AddAttribute(0, 3, GL_FLOAT, 3 * sizeof(float), 0, false);

        // TODO: pass shader to renderer instead of initializing here...
        _shader = std::make_unique<OpenGLShader>();

        // Compile and bind our shader
        const auto& vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 inPosition;

            out vec3 outPosition;
            
            void main()
            {
                outPosition = inPosition;
                gl_Position = vec4(inPosition, 1.0);
            }
        )";


        const auto& fragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 outColor;

            in vec3 outPosition;
            
            void main()
            {
                outColor = vec4(outPosition * 0.5 + 0.5, 1.0);
            }
        )";

        _shader->Compile(vertexSrc, fragmentSrc);
        _shader->Bind();
    }

    void OpenGLRenderer::Shutdown()
    {
        glDeleteVertexArrays(1, &_vertexArray);
    }

    void OpenGLRenderer::BeginFrame()
    {
        // Clear screen at the beginning of our frame
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndFrame()
    {
        // Draw at the end of our frame if we have anything to draw
        if (_vertexArray && _vertexBuffer->GetCount() > 0)
        {
            glDrawElements(GL_TRIANGLES, _indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
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
        glClearColor(color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(const Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale)
    {
        // Set vertex buffer
        const auto& meshVertBuff = mesh.GetVertexBuffer();
        _vertexBuffer->SetData(meshVertBuff);

        // Set index buffer
        const auto& meshIndexBuff = mesh.GetIndexBuffer();
        _indexBuffer->SetData(meshIndexBuff);
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
