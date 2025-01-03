#pragma once
#include "OpenGLContext.h"
#include "OpenGLBuffers.h"
#include "OpenGLShader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <Core.h>

namespace OpenGLRendering
{
    class OpenGLRenderer : public Toybox::IRenderer
    {
    public:
        void Initialize(const std::weak_ptr<Toybox::IWindow>& context) override;
        void Shutdown() override;

        void BeginFrame() override;
        void EndFrame() override;

        void ClearScreen() override;
        void Draw(const Toybox::Color& color) override;
        void Draw(const Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;
        void Draw(const Toybox::Texture& texture, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;
        void Draw(const std::string& text, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;

        void SetViewport(const Toybox::Vector2I& screenPos, const Toybox::Size& size) override;
        void SetVSyncEnabled(const bool& enabled) override;

        std::string GetRendererName() const override;

    private:
        std::unique_ptr<OpenGLShader> _shader;
        std::unique_ptr<OpenGLContext> _context;
        std::unique_ptr<OpenGLVertexArray> _vertArray;
    };
}

