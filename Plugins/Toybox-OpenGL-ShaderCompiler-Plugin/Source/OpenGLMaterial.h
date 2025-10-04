#pragma once
#include "IBindable.h"
#include "OpenGLTexture.h"
#include <Tbx/Graphics/Material.h>

namespace OpenGLRendering
{
    class OpenGLShader
    {
    public:
        ~OpenGLShader();
        void Compile(const Tbx::Shader& shader, Tbx::uint programId);
        void UploadUniform(const Tbx::ShaderUniform& data) const;
        void Detach() const;

    private:
        Tbx::uint _programId = -1;
        Tbx::uint _shaderId = -1;
        Tbx::ShaderType _type = Tbx::ShaderType::None;
    };

    class OpenGLMaterial final : public IBindable
    {
    public:
        OpenGLMaterial() = default;
        ~OpenGLMaterial() override;

        void Bind() const override;
        void Unbind() const override;

        void Upload(const Tbx::Material& material);
        void UploadUniform(const Tbx::ShaderUniform& data) const;

    private:
        Tbx::uint _materialGLId = -1;
        std::vector<OpenGLShader> _shaders = {};
    };

    static const OpenGLMaterial& DefaultMaterial = {};

    class OpenGLMaterialInstance final : public IBindable
    {
    public:
        OpenGLMaterialInstance()
            : _material(DefaultMaterial) {}
        OpenGLMaterialInstance(const OpenGLMaterial& material)
            : _material(material) {}

        void Bind() const override;
        void Unbind() const override;

        void Upload(const Tbx::MaterialInstance& material);
        void SetUniform(const Tbx::ShaderUniform& data) const;

    private:
        const OpenGLMaterial& _material;
        std::vector<OpenGLTexture> _textures = {};
    };
}

