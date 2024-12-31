#pragma once
#include "OpenGLBuffer.h"
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
        void Draw(Toybox::Color color) override;
        void Draw(Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;
        void Draw(const Toybox::Texture& texture, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;
        void Draw(const std::string& text, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale) override;

        void SetViewport(const Toybox::Vector2I& screenPos, const Toybox::Size& size) override;
        void SetVSyncEnabled(const bool& enabled) override;

        std::string GetRendererName() const override;

    private:
        std::shared_ptr<OpenGLBuffer> _buffer = nullptr;
        std::weak_ptr<Toybox::IWindow> _context;

        Toybox::uint _vertexArray;
        Toybox::uint _vertexBuffer;
        Toybox::uint _indexBuffer;
        //GLuint _shaderProgram;

        //GLuint LoadShader(const std::string& vertexSrc, const std::string& fragmentSrc) const;
        //GLuint LoadTexture(const std::string& filepath);
    };
}

