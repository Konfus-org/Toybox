#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::SetContext(const std::weak_ptr<Tbx::IWindow>& context)
    {
        _context.Set(context);
    }

    void OpenGLRenderer::SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size)
    {
        glViewport(screenPos.X, screenPos.Y, size.Width, size.Height);
    }

    void OpenGLRenderer::SetVSyncEnabled(const bool& enabled)
    {
        _context.SetSwapInterval(enabled);
    }

    void OpenGLRenderer::SetShader(const Tbx::Shader& shader)
    {
        // TODO: we need to check if the shader already exists and just bind it if it does!
        auto& glShader = _shaders.emplace_back();
        glShader.Compile(shader);
        glShader.Bind();
    }

    void OpenGLRenderer::UploadShaderData(const Tbx::ShaderData& data)
    {
        // TODO: Need to keep track of active shader and upload data to it!
        const auto& glShader = _shaders.back();
        glShader.UploadData(data);
    }

    void OpenGLRenderer::Flush()
    {
        _vertArraysToDraw.clear();
        _shaders.clear();
    }

    void OpenGLRenderer::Clear()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
}
