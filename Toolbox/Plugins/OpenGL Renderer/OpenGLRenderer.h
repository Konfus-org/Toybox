#pragma once
#include "OpenGLContext.h"
#include "OpenGLBuffers.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <Tbx/App/Render Pipeline/IRenderer.h>

namespace OpenGLRendering
{
    class OpenGLRenderer : public Tbx::IRenderer
    {
    public:
        void SetContext(const std::weak_ptr<Tbx::IWindow>& context) override;
        void SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size) override;
        void SetVSyncEnabled(const bool& enabled) override;

        void UploadTexture(const Tbx::Texture& texture, const Tbx::uint& slot) override;
        void UploadShader(const Tbx::Shader& shader) override;
        void UploadShaderData(const Tbx::ShaderData& data) override;

        void Flush() override;
        void Clear(const Tbx::Color& color = Tbx::Color::Black()) override;

        void BeginDraw() override;
        void EndDraw() override;

        void Draw(const Tbx::Mesh& mesh, const Tbx::Material& material) override;

    private:
        std::vector<OpenGLShader> _shaders;
        std::vector<OpenGLTexture> _textures;
        OpenGLContext _context;
    };
}

