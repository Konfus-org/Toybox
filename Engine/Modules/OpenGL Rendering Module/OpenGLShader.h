#pragma once
#include <Core.h>

class OpenGLShader : public Toybox::Shader
{
public:
    ~OpenGLShader() final;

    void Compile(const std::string& vertexShader, const std::string& fragmentShader) override;
    void Bind() const override;
    void Unbind() const override;
    
private:
    Toybox::uint _rendererId;
};

