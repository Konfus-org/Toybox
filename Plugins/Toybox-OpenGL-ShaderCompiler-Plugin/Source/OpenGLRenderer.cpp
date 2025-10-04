#include "OpenGLRenderer.h"
#include <Tbx/Debug/Tracers.h>
#include <glad/glad.h>

#define TBX_VERBOSE_ENABLED

namespace OpenGLRendering
{
    void OpenGLRenderer::Process(const Tbx::RenderCommandBuffer& buffer)
    {
        // Process all draw commands in frame buffer
        for (const auto& cmd : buffer.Commands)
        {
            switch (cmd.Type)
            {
                case Tbx::RenderCommandType::Clear:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Clearing color buffer");

                    const auto& color = std::any_cast<const Tbx::RgbaColor&>(cmd.Payload);
                    Clear(color);
                    break;
                }
                case Tbx::RenderCommandType::UploadMaterial:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Compiling material");

                    const auto& material = std::any_cast<const Tbx::MaterialInstance&>(cmd.Payload);
                    UploadMaterial(material);
                    break;
                }
                case Tbx::RenderCommandType::SetMaterial:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Setting material");

                    const auto& material = std::any_cast<const Tbx::MaterialInstance&>(cmd.Payload);
                    SetMaterial(material);
                    break;
                }
                case Tbx::RenderCommandType::SetUniform:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Uploading material data");

                    const auto& uniform = std::any_cast<const Tbx::ShaderUniform&>(cmd.Payload);
                    SetUniform(uniform);
                    break;
                }
                case Tbx::RenderCommandType::UploadMesh:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Uploading mesh");

                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.Payload);
                    UploadMesh(mesh);
                    break;
                }
                case Tbx::RenderCommandType::DrawMesh:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Drawing mesh");

                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.Payload);
                    DrawMesh(mesh);
                    break;
                }
                case Tbx::RenderCommandType::SetResolution:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Setting resolution");

                    const auto& resolution = std::any_cast<const Tbx::Size&>(cmd.Payload);
                    SetResolution(resolution);
                    break;
                }
                case Tbx::RenderCommandType::SetViewport:
                {
                    TBX_TRACE_VERBOSE("GL Rendering: Setting viewport");

                    const auto& viewport = std::any_cast<const Tbx::Viewport&>(cmd.Payload);
                    SetViewport(viewport);
                    break;
                }
                default:
                {
                    TBX_ASSERT(false, "GL Rendering: Unknown draw command type!");
                    break;
                }
            }
        }

        FinalizeFrame();
    }

    void OpenGLRenderer::Flush()
    {
        _meshCache.clear();
        _materialInstanceCache.clear();
        _materialCache.clear();
        _activeMaterial = Tbx::Uid::Invalid;
        _activeMesh = Tbx::Uid::Invalid;
        Clear(Tbx::RgbaColor::Black);
    }

    void OpenGLRenderer::Clear(const Tbx::RgbaColor& color)
    {
        glClearColor(color.R, color.G, color.B, color.A);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::SetResolution(const Tbx::Size& size)
    {
        _resolution = size;
    }

    Tbx::Size OpenGLRenderer::GetResolution()
    {
        return _resolution;
    }

    void OpenGLRenderer::SetViewport(const Tbx::Viewport& viewport)
    {
        _viewport = viewport;
        glViewport((GLint)viewport.Position.X, (GLint)viewport.Position.Y, viewport.Extends.Width, viewport.Extends.Height);
    }

    Tbx::Viewport OpenGLRenderer::GetViewport()
    {
        return _viewport;
    }

    void OpenGLRenderer::FinalizeFrame()
    {
        if (_activeMaterial != Tbx::Uid::Invalid)
        {
            const auto materialIt = _materialInstanceCache.find(_activeMaterial);
            if (materialIt != _materialInstanceCache.end())
            {
                materialIt->second.Unbind();
            }
            _activeMaterial = Tbx::Uid::Invalid;
        }

        _activeMesh = Tbx::Uid::Invalid;
    }

    void OpenGLRenderer::DrawMesh(const Tbx::Mesh& mesh)
    {
        const auto& meshId = mesh.Id;
        const auto meshIt = _meshCache.find(meshId);
        TBX_ASSERT(meshIt != _meshCache.end(), "GL Rendering: Attempted to draw a mesh that has not been uploaded!");
        if (meshIt == _meshCache.end())
        {
            return;
        }

        auto& glMesh = meshIt->second;
        if (_activeMesh != meshId)
        {
            glMesh.Bind();
            _activeMesh = meshId;
        }

        glDrawElements(GL_TRIANGLES, glMesh.GetIndexBuffer().GetCount(), GL_UNSIGNED_INT, 0);
    }

    void OpenGLRenderer::UploadMesh(const Tbx::Mesh& mesh)
    {
        // TODO: Implement mesh instancing
        const auto& meshId = mesh.Id;
        auto [meshIt, inserted] = _meshCache.try_emplace(meshId);
        if (!inserted)
        {
            return;
        }

        auto& glMesh = meshIt->second;
        glMesh.Bind();
        glMesh.UploadVertexBuffer(mesh.Vertices);
        glMesh.UploadIndexBuffer(mesh.Indices);

        // Uploading the mesh binds its VAO which dirties the cached active mesh.
        // Force the next draw call to explicitly rebind whichever mesh it needs.
        _activeMesh = Tbx::Uid::Invalid;
    }

    void OpenGLRenderer::SetMaterial(const Tbx::MaterialInstance& mat)
    {
        const auto& materialInstanceId = mat.Id;
        if (_activeMaterial == materialInstanceId)
        {
            return;
        }

        if (_activeMaterial != Tbx::Uid::Invalid)
        {
            const auto previousIt = _materialInstanceCache.find(_activeMaterial);
            if (previousIt != _materialInstanceCache.end())
            {
                previousIt->second.Unbind();
            }
        }

        const auto materialIt = _materialInstanceCache.find(materialInstanceId);
        TBX_ASSERT(materialIt != _materialInstanceCache.end(), "GL Rendering: Attempted to set an unknown material instance!");
        if (materialIt == _materialInstanceCache.end())
        {
            _activeMaterial = Tbx::Uid::Invalid;
            return;
        }

        materialIt->second.Bind();
        _activeMaterial = materialInstanceId;
    }

    void OpenGLRenderer::UploadMaterial(const Tbx::MaterialInstance& materialInstance)
    {
        const auto& materialId = materialInstance.InstanceOf->Id;
        auto [materialIt, materialInserted] = _materialCache.try_emplace(materialId);
        if (materialInserted)
        {
            materialIt->second.Upload(*materialInstance.InstanceOf);
        }

        const auto& materialInstanceId = materialInstance.Id;
        auto [materialInstanceIt, materialInstanceInserted] =
            _materialInstanceCache.try_emplace(materialInstanceId, materialIt->second);
        if (materialInstanceInserted)
        {
            materialInstanceIt->second.Upload(materialInstance);

            // Uploading the material instance binds and unbinds the shader program.
            // This leaves GL without an active program, so invalidate the cached
            // material to force the next SetMaterial call to rebind the state.
            _activeMaterial = Tbx::Uid::Invalid;
        }
    }

    void OpenGLRenderer::SetUniform(const Tbx::ShaderUniform& data)
    {
        if (_activeMaterial == Tbx::Uid::Invalid)
        {
            TBX_ASSERT(false, "GL Rendering: Attempted to set a uniform without an active material instance!");
            return;
        }

        const auto materialIt = _materialInstanceCache.find(_activeMaterial);
        TBX_ASSERT(materialIt != _materialInstanceCache.end(), "GL Rendering: Attempted to set a uniform on an unknown material instance!");
        if (materialIt == _materialInstanceCache.end())
        {
            return;
        }

        materialIt->second.SetUniform(data);
    }
}
