#include "OpenGLRenderer.h"

namespace OpenGLRendering
{

    void OpenGLRenderer::SetContext(const std::weak_ptr<Tbx::IWindow>& context)
    {
        _context.Set(context);

        // TODO: pass shader source to renderer instead of hard coding here...
        // Compile and bind our shader
        _shader = std::make_unique<OpenGLShader>();
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

    void OpenGLRenderer::Flush()
    {
        _vertArraysToDraw.clear();
    }

    void OpenGLRenderer::BeginDraw()
    {
        // Clear screen at the beginning of our frame
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndDraw()
    {
        // Draw at the end of our frame if we have anything to draw
        for (const auto& vertArrayToDraw : _vertArraysToDraw)
        {
            vertArrayToDraw.Bind();
            glDrawElements(GL_TRIANGLES, vertArrayToDraw.GetIndexCount(), GL_UNSIGNED_INT, nullptr);
        }

        // Finally swap buffers
        _context.SwapBuffers();
    }

    void OpenGLRenderer::Clear()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        _vertArraysToDraw.clear();
    }

    void OpenGLRenderer::Draw(const Tbx::Color& color)
    {
        glClearColor(color.R, color.G, color.B, color.A);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(const Tbx::Mesh& mesh)
    {
        // Emplace and bind
        auto& vertArray = _vertArraysToDraw.emplace_back();
        vertArray.Bind();

        // Add vertex buffer
        const auto& meshVertexBuffer = mesh.GetVertexBuffer();
        vertArray.AddVertexBuffer(meshVertexBuffer);

        // Set index buffer
        const auto& meshIndexBuffer = mesh.GetIndexBuffer();
        vertArray.SetIndexBuffer(meshIndexBuffer);
    }

    void OpenGLRenderer::Draw(const Tbx::Texture& texture)
    {
        // TODO: Draw texture
    }

    void OpenGLRenderer::Draw(const std::string& text)
    {
        // TODO: Draw text
    }

    void OpenGLRenderer::SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size)
    {
        glViewport(screenPos.X, screenPos.Y, size.Width, size.Height);
    }

    void OpenGLRenderer::SetVSyncEnabled(const bool& enabled)
    {
        _context.SetSwapInterval(enabled);
    }

    std::string OpenGLRenderer::GetRendererName() const
    {
        return "OpenGL";
    }
}
