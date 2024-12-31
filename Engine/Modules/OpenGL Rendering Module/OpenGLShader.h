#pragma once
#include <Core.h>

namespace OpenGLRendering
{
    class OpenGLShader : public Toybox::IShader
    {
    public:
        ~OpenGLShader() final;

        void Compile(const std::string& vertexShader, const std::string& fragmentShader) override;
        void Bind() const override;
        void Unbind() const override;

    private:
        Toybox::uint _rendererId = -1;
    };
}

