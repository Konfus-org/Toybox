#include "OpenGLRenderer.h"
#include "OpenGLBuffers.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::SetContext(const std::weak_ptr<Tbx::IRenderSurface>& context)
    {
        _context.Set(context);
    }

    void OpenGLRenderer::SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size)
    {
        TBX_VERBOSE("OpenGl Renderer: Set viewport size to {}...\n", size.ToString());

        _viewportSize = size;
        glViewport(screenPos.X, screenPos.Y, size.Width, size.Height);
    }

    void OpenGLRenderer::SetResolution(const Tbx::Size& size)
    {
        TBX_VERBOSE("OpenGl Renderer: Set resolution to {}...\n", size.ToString());

        _resolution = size;

        //glGenFramebuffers(1, &_frameBuf);
        //glBindFramebuffer(GL_FRAMEBUFFER, _frameBuf);

        //// Create color attachment
        //glGenTextures(1, &_colorTex);
        //glBindTexture(GL_TEXTURE_2D, _colorTex);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.Width, size.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);

        //// Depth buffer
        //glGenRenderbuffers(1, &_depthBuf);
        //glBindRenderbuffer(GL_RENDERBUFFER, _depthBuf);
        //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.Width, size.Height);
        //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _depthBuf);

        //// Check framebuffer status
        //if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        //{
        //    TBX_ASSERT(false, "Framebuffer is not complete!");
        //}

        //// Unbind the framebuffer
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLRenderer::SetVSyncEnabled(const bool& enabled)
    {
        TBX_VERBOSE("OpenGl Renderer: Set vsync to {}...\n", enabled);
        _context.SetSwapInterval(enabled);
    }

    void OpenGLRenderer::Flush()
    {
        TBX_VERBOSE("OpenGl Renderer: flushing...\n");

        _shaders.clear();
        _textures.clear();
        glFlush();
    }

    void OpenGLRenderer::Clear(const Tbx::Color& color)
    {
        TBX_VERBOSE("OpenGl Renderer: clearing to {}...\n", color.ToString());

        glClearColor(color.R, color.G, color.B, color.A);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::BeginDraw()
    {
        TBX_VERBOSE("OpenGl Renderer: BEGIN DRAW");

        // Clear whatever was on screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndDraw()
    {
        TBX_VERBOSE("OpenGl Renderer: END DRAW \n");

        //// Blit scene FBO to screen
        //glBindFramebuffer(GL_READ_FRAMEBUFFER, _frameBuf);
        //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _frameBuf);
        //glBlitFramebuffer(
        //    0, 0, _resolution.Width, _resolution.Height,
        //    0, 0, _viewportSize.Width, _viewportSize.Height,
        //    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_LINEAR);

        //// Unbind framebuffers
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Swap our buffers
        _context.SwapBuffers();
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
        ProcessData(_lastDrawnData);
    }

    void OpenGLRenderer::ProcessData(const Tbx::RenderData& data)
    {
        const auto& command = data.GetCommand();
        const auto& payload = data.GetPayload();
        switch (command)
        {
            case Tbx::RenderCommand::None:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing none cmd...");
                break;
            }
            case Tbx::RenderCommand::Clear:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing clear cmd...");

                const auto& color = std::any_cast<const Tbx::Color&>(payload);
                Clear(color);
                break;
            }
            case Tbx::RenderCommand::CompileMaterial:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing compile material cmd...");

                const auto& material = std::any_cast<const Tbx::Material&>(payload);

                CompileShader(material.GetShader());

                int textureSlot = 0;
                for (const auto& texture : material.GetTextures())
                {
                    UploadTexture(texture, textureSlot);
                    textureSlot++;
                }

                break;
            }
            case Tbx::RenderCommand::UploadMaterialShaderData:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing upload material data cmd...");

                const auto& shaderData = std::any_cast<const Tbx::ShaderData&>(payload);
                UploadShaderData(shaderData);
                break;
            }
            case Tbx::RenderCommand::SetMaterial:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing set material cmd...");

                const auto& material = std::any_cast<const Tbx::Material&>(payload);
                SetMaterial(material);
                break;
            }
            case Tbx::RenderCommand::RenderMesh:
            {
                TBX_VERBOSE("OpenGl Renderer: Processing mesh cmd...");

                _lastDrawnData = data;
                const auto& mesh = std::any_cast<const Tbx::Mesh&>(payload);
                Draw(mesh);
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

    void OpenGLRenderer::SetMaterial(const Tbx::Material& material)
    {
        const auto& materialShaderId = material.GetShader().GetId();

        const auto& glShader = std::ranges::find_if(_shaders, [&](const OpenGLShader& shader)
        {
            return shader.GetAssociatedAssetId() == materialShaderId;
        });

        glShader->Bind();

        //glShader->UploadData(Tbx::ShaderData("colorUni", material.GetColor(), Tbx::ShaderDataType::Float4));

        //for (const auto& texture : material.GetTextures())
        //{
        //    const auto& glTexture = std::find_if(_textures.begin(), _textures.end(),
        //        [&](const OpenGLTexture& glt) { return glt.GetAssociatedAssetId() == texture.GetId(); });
        //    glTexture->Bind();
        //}
    }

    void OpenGLRenderer::UploadShaderData(const Tbx::ShaderData& data)
    {
        const auto& glShader = _shaders.back();
        glShader.UploadData(data);
    }
}
