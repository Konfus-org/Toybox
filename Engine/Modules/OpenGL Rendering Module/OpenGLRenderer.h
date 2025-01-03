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
        void Initialize(const std::weak_ptr<Tbx::IWindow>& context) override;
        void Shutdown() override;

        void BeginFrame() override;
        void EndFrame() override;

        void ClearScreen() override;
        void Draw(const Tbx::Color& color) override;
        void Draw(const Tbx::Mesh& mesh, const Tbx::Vector3& worldPos, const Tbx::Quaternion& rotation, const Tbx::Scale& scale) override;
        void Draw(const Tbx::Texture& texture, const Tbx::Vector3& worldPos, const Tbx::Quaternion& rotation, const Tbx::Scale& scale) override;
        void Draw(const std::string& text, const Tbx::Vector3& worldPos, const Tbx::Quaternion& rotation, const Tbx::Scale& scale) override;

        void SetViewport(const Tbx::Vector2I& screenPos, const Tbx::Size& size) override;
        void SetVSyncEnabled(const bool& enabled) override;

        std::string GetRendererName() const override;

    private:
        std::unique_ptr<OpenGLShader> _shader;
        std::unique_ptr<OpenGLContext> _context;
        std::unique_ptr<OpenGLVertexArray> _vertArray;
    };
}

