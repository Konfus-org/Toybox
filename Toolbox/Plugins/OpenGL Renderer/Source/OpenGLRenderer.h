#pragma once
#include "OpenGLContext.h"
#include "OpenGLBuffers.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/App/Render Pipeline/RenderQueue.h>
#include <glad/glad.h>
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
        void SetVSyncEnabled(const bool& enabled) final;

        void ProcessData(const Tbx::RenderData& data) final;

        void UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot) final;
        void UploadShader(const Tbx::Shader& shader) final;
        void UploadShaderData(const Tbx::ShaderData& data) final;

        void Flush() final;
        void Clear(const Tbx::Color& color = Tbx::Color::Black()) final;

        void BeginDraw() final;
        void EndDraw() final;

        void Draw(const Tbx::Mesh& mesh, const Tbx::Material& material) final;
        void Redraw() final;

    private:
        Tbx::RenderData _lastDrawnData;
        std::vector<OpenGLShader> _shaders;
        std::vector<OpenGLTexture> _textures;
        OpenGLContext _context;
    };
}

