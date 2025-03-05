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

    void OpenGLRenderer::SetTexture(const Tbx::Texture& texture)
    {
        OpenGLTexture glTexture;
        glTexture.SetData(texture);
        glTexture.Bind();
    }

    void OpenGLRenderer::SetShader(const Tbx::Shader& shader)
    {
        // TODO: we need to check if the shader already exists and just bind it if it does! 
        // Then we can add the concept of "active shader" to the renderer and batch things by shader
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
        _shaders.clear();
    }

    void OpenGLRenderer::Clear()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::BeginDraw()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndDraw()
    {
        _context.SwapBuffers();
    }

    void OpenGLRenderer::Draw(const Tbx::Color& color)
    {
        glClearColor(color.R, color.G, color.B, color.A);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(const Tbx::Mesh& mesh)
    {
        OpenGLVertexArray vertArray;
        vertArray.Bind();

        const auto& meshVertexBuffer = mesh.GetVertexBuffer();
        vertArray.AddVertexBuffer(meshVertexBuffer);
        const auto& meshIndexBuffer = mesh.GetIndexBuffer();
        vertArray.SetIndexBuffer(meshIndexBuffer);

        glDrawElements(GL_TRIANGLES, vertArray.GetIndexCount(), GL_UNSIGNED_INT, nullptr);
    }
}
