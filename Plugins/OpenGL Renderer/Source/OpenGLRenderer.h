#pragma once
#include "OpenGLContext.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLRenderer : public Tbx::IRenderer
    {
    public:
        OpenGLRenderer() = default;
        ~OpenGLRenderer() override = default;

        void SetContext(const std::weak_ptr<Tbx::IRenderSurface>& context) final;
        void SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size) final;
        void SetResolution(const Tbx::Size& size) final;
        void SetVSyncEnabled(const bool& enabled) final;

        void SetMaterial(const Tbx::Material& material) final;
        void UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot) final;
        void CompileShader(const Tbx::Shader& shader) final;
        void UploadShaderData(const Tbx::ShaderData& data) final;

        void ProcessData(const Tbx::RenderData& data) final;

        void Flush() final;
        void Clear(const Tbx::Color& color) final;

        void BeginDraw() final;
        void EndDraw() final;

        void Draw(const Tbx::Mesh& mesh) final;
        void Redraw() final;

        Tbx::Size GetViewportSize() const { return _viewportSize; }

    private:
        Tbx::uint _frameBuf = 0;
        Tbx::uint _depthBuf = 0;
        Tbx::uint _colorTex = 0;

        Tbx::Size _resolution = { 1920, 1080 };
        Tbx::Size _viewportSize = { 1920, 1080 };

        Tbx::RenderData _lastDrawnData = {};

        std::vector<OpenGLShader> _shaders = {};
        std::vector<OpenGLTexture> _textures = {};

        OpenGLContext _context = {};
    };
}

