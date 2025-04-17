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
                const auto& color = std::any_cast<Tbx::Color&>(payload);
                Clear(color);
                break;
            }
            case Tbx::RenderCommand::CompileMaterial:
            {
                const auto& material = std::any_cast<std::shared_ptr<Tbx::Material>>(payload);

                CompileShader(material->GetShader());

                int textureSlot = 0;
                for (const auto& texture : material->GetTextures())
                {
                    UploadTexture(texture, textureSlot);
                    textureSlot++;
                }

                break;
            }
            case Tbx::RenderCommand::UploadMaterialShaderData:
            {
                const auto& shaderData = std::any_cast<Tbx::ShaderData&>(payload);
                UploadShaderData(shaderData);
                break;
            }
            case Tbx::RenderCommand::SetMaterial:
            {
                const auto& materialData = std::any_cast<std::shared_ptr<Tbx::Material>>(payload);
                SetMaterial(*materialData);
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

    void OpenGLRenderer::CompileShader(const Tbx::Shader& shader)
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

        const auto& glShader = std::find_if(_shaders.begin(), _shaders.end(),
            [&](const OpenGLShader& shader) { return shader.GetAssociatedAssetId() == materialShaderId; });

        glShader->Bind();

        for (const auto& texture : material.GetTextures())
        {
            const auto& glTexture = std::find_if(_textures.begin(), _textures.end(),
                [&](const OpenGLTexture& glt) { return glt.GetAssociatedAssetId() == texture.GetId(); });
            glTexture->Bind();
        }
    }

    void OpenGLRenderer::Draw(const Tbx::Mesh& mesh)
    {
        OpenGLVertexArray vertArray;
        vertArray.Bind();

        const auto& meshVertexBuffer = mesh.GetVertices();
        vertArray.AddVertexBuffer(meshVertexBuffer);
        const auto& meshIndexBuffer = mesh.GetIndices();
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
