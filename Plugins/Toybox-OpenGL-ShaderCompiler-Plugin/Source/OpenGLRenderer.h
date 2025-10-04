#pragma once
#include "OpenGLMaterial.h"
#include "OpenGLMesh.h"
#include <Tbx/Graphics/IRenderer.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Viewport.h>

namespace OpenGLRendering
{
    class OpenGLRenderer : public Tbx::IRenderer
    {
    public:
        void Process(const Tbx::RenderCommandBuffer& buffer) override;

        void Flush() override;
        void Clear(const Tbx::RgbaColor& color);

        void SetResolution(const Tbx::Size& size);
        Tbx::Size GetResolution();

        void SetViewport(const Tbx::Viewport& viewPort);
        Tbx::Viewport GetViewport();

        Tbx::GraphicsApi GetApi() const override
        {
            return Tbx::GraphicsApi::OpenGL;
        }

    private:
        void FinalizeFrame();
        void DrawMesh(const Tbx::Mesh& mesh);
        void UploadMesh(const Tbx::Mesh& mesh);
        void SetMaterial(const Tbx::MaterialInstance& mat);
        void UploadMaterial(const Tbx::MaterialInstance& materialInstance);
        void SetUniform(const Tbx::ShaderUniform& data);

        Tbx::Uid _activeMaterial = Tbx::Uid::Invalid;
        Tbx::Uid _activeMesh = Tbx::Uid::Invalid;
        std::unordered_map<Tbx::Uid, OpenGLMesh> _meshCache = {};
        std::unordered_map<Tbx::Uid, OpenGLMaterial> _materialCache = {};
        std::unordered_map<Tbx::Uid, OpenGLMaterialInstance> _materialInstanceCache = {};
        Tbx::Size _resolution = {};
        Tbx::Viewport _viewport = {};
    };
}

