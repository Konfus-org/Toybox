#pragma once
#include "OpenGLContext.h"
#include "OpenGLBuffers.h"
#include "OpenGLShader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <TbxCore.h>

namespace OpenGLRendering
{
    class OpenGLRenderer : public Tbx::IRenderer
    {
    public:
        void SetContext(const std::weak_ptr<Tbx::IWindow>& context) override;
        void Flush() override;

        void BeginDraw() override;
        void EndDraw() override;

        void Clear() override;
        void Draw(const Tbx::Color& color) override;
        void Draw(const Tbx::Mesh& mesh) override;
        void Draw(const Tbx::Texture& texture) override;
        void Draw(const std::string& text) override;

        void SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size) override;
        void SetVSyncEnabled(const bool& enabled) override;

        std::string GetRendererName() const override;

    private:
        std::vector<OpenGLVertexArray> _vertArraysToDraw;
        OpenGLContext _context;

        // TODO: Pass shader source to renderer instead of hard coding here...
        std::unique_ptr<OpenGLShader> _shader;
    };
}

