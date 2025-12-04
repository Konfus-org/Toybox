#pragma once
#include <Tbx/Graphics/Shader.h>
#include <Tbx/Graphics/GraphicsResources.h>

namespace Tbx::Plugins::OpenGLRendering
{
    class OpenGLShader : public ShaderResource
    {
    public:
        OpenGLShader(const Shader& shader);
        ~OpenGLShader();

        void Activate() override {}
        void Release() override {}

    private:
        ShaderType _type = ShaderType::None;
    };

    class OpenGLShaderProgram final : public ShaderProgramResource
    {
    public:
        OpenGLShaderProgram(const std::vector<Ref<ShaderResource>>& shaders);
        ~OpenGLShaderProgram();

        void Activate() override;
        void Release() override;

        void Upload(const ShaderUniform& uniform) override;
    };
}

