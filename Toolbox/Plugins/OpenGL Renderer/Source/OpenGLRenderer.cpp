#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::SetContext(const std::weak_ptr<Tbx::IRenderSurface>& context)
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

    void OpenGLRenderer::ProcessData(const Tbx::RenderData& data)
    {
        const auto& command = data.GetCommand();
        const auto& payload = data.GetPayload();
        switch (command)
        {
            case Tbx::RenderCommand::None:
            {
                break;
            }
            case Tbx::RenderCommand::Clear:
            {
                const auto& colorData = std::any_cast<Tbx::Color&>(payload);
                Clear(colorData);
                break;
            }
            case Tbx::RenderCommand::UploadShader:
            {
                const auto& shaderData = std::any_cast<Tbx::Shader&>(payload);
                UploadShader(shaderData);
                break;
            }
            case Tbx::RenderCommand::UploadTexture:
            {
                const auto& textureData = std::any_cast<Tbx::TextureRenderData&>(payload);
                UploadTexture(textureData.GetTexture(), textureData.GetSlot());
                break;
            }
            case Tbx::RenderCommand::UploadShaderData:
            {
                const auto& shaderData = std::any_cast<Tbx::ShaderData&>(payload);
                UploadShaderData(shaderData);
                break;
            }
            case Tbx::RenderCommand::SetMaterial:
            {
                const auto& materialData = std::any_cast<Tbx::Material&>(payload);
                SetMaterial(materialData);
                break;
            }
            case Tbx::RenderCommand::RenderMesh:
            {
                _lastDrawnData = data;
                const auto& meshData = std::any_cast<Tbx::Mesh&>(payload);
                Draw(meshData);
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Unknown render command type.");
                break;
            }
        }
    }

    void OpenGLRenderer::UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot)
    {
        auto& lastGlTexture = _textures.emplace_back();
        lastGlTexture.SetData(texture, slot);
    }

    void OpenGLRenderer::UploadShader(const Tbx::Shader& shader)
    {
        auto& glShader = _shaders.emplace_back();
        glShader.Compile(shader);
    }

    void OpenGLRenderer::UploadShaderData(const Tbx::ShaderData& data)
    {
        const auto& glShader = _shaders.back();
        glShader.UploadData(data);
    }

    void OpenGLRenderer::Flush()
    {
        _shaders.clear();
        _textures.clear();
        glFlush();
    }

    void OpenGLRenderer::Clear(const Tbx::Color& color)
    {
        glClearColor(color.R, color.G, color.B, color.A);
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

    void OpenGLRenderer::SetMaterial(const Tbx::Material& material)
    {
        const auto& materialShaderId = material.GetShader().GetId();
        const auto& materialTextureId = material.GetTextures().front().GetId();

        const auto& glShader = std::find_if(_shaders.begin(), _shaders.end(),
            [&](const OpenGLShader& shader) { return shader.GetAssociatedAssetId() == materialShaderId; });

        const auto& glTexture = std::find_if(_textures.begin(), _textures.end(),
            [&](const OpenGLTexture& texture) { return texture.GetAssociatedAssetId() == materialTextureId; });

        glShader->Bind();
        glTexture->Bind();
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

    void OpenGLRenderer::Redraw()
    {
        const auto& command = _lastDrawnData.GetCommand();
        const auto& payload = _lastDrawnData.GetPayload();
        switch (command)
        {
            case Tbx::RenderCommand::None:
            {
                break;
            }
            case Tbx::RenderCommand::RenderMesh:
            {
                const auto& meshData = std::any_cast<Tbx::MeshRenderData>(payload);
                Draw(meshData.GetMesh());
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Redraw only supports RenderMesh command.");
                break;
            }
        }
    }
}
