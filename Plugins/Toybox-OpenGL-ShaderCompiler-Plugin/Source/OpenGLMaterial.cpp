#include "OpenGLMaterial.h"
#include <Tbx/Math/Vectors.h>
#include <Tbx/Math/Mat4x4.h>
#include <Tbx/Graphics/Color.h>
#include <Tbx/Debug/Asserts.h>
#include <glad/glad.h>
#include <vector>

namespace OpenGLRendering
{
    ///// Helpers //////////////////////////////////////////////////////////////////
    static Tbx::uint GetUniformLocation(const std::string name, Tbx::uint programId)
    {
        GLint location = glGetUniformLocation(programId, name.c_str());
        //TBX_ASSERT(location != -1, "Invalid uniform location: {}", name);
        return location;
    }

    static void UploadUniformInt(const std::string& name, int value, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1i(location, value);
    }

    static void UploadUniformIntArray(const std::string& name, std::vector<int> values, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1iv(location, (Tbx::uint32)values.size(), values.data());
    }

    static void UploadUniformFloat(const std::string& name, float value, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1f(location, value);
    }

    static void UploadUniformFloat2(const std::string& name, const Tbx::Vector2& value, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform2f(location, value.X, value.Y);
    }

    static void UploadUniformFloat3(const std::string& name, const Tbx::Vector3& value, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform3f(location, value.X, value.Y, value.Z);
    }

    static void UploadUniformFloat4(const std::string& name, const Tbx::RgbaColor& value, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform4f(location, value.R, value.G, value.B, value.A);
    }

    // TODO: Implement when needed... Toybox doesn't have a mat3 yet...
    ////static void UploadUniformMat3(const std::string& name, const Tbx::Matrix& matrix, Tbx::uint rendererId)
    ////{
    ////	GLint location = glGetUniformLocation(rendererId, name.c_str());
    ////	glUniformMatrix3fv(location, 1, GL_FALSE, matrix.Values.data());
    ////}

    static void UploadUniformMat4(const std::string& name, const Tbx::Mat4x4& matrix, Tbx::uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix.Values.data());
    }

    ///// Shader //////////////////////////////////////////////////////////////////
    OpenGLShader::~OpenGLShader()
    {
        glDeleteShader(_shaderId);
    }

    void OpenGLShader::Compile(const Tbx::Shader& shader, Tbx::uint programId)
    {
        _programId = programId;
        _type = shader.Type;

        // Create a shader handle
        _shaderId;
        if (shader.Type == Tbx::ShaderType::Vertex)
        {
            _shaderId = glCreateShader(GL_VERTEX_SHADER);
        }
        else if (shader.Type == Tbx::ShaderType::Fragment)
        {
            _shaderId = glCreateShader(GL_FRAGMENT_SHADER);
        }
        else
        {
            TBX_ASSERT(false, "GL Rendering: Unsupported shader type.");
        }

        // Send the shader source code to GL
        // Note that std::string's .c_str is NULL character terminated.
        const auto* source = shader.Source.c_str();
        glShaderSource(_shaderId, 1, &source, nullptr);

        // Compile the vertex shader
        glCompileShader(_shaderId);

        GLint isCompiled = 0;
        glGetShaderiv(_shaderId, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) // Check if we failed compilation
        {
            // Get the error
            GLint maxLength = 0;
            glGetShaderiv(_shaderId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(_shaderId, maxLength, &maxLength, &infoLog[0]);

            // Cleanup
            glDeleteShader(_shaderId);

            // Assert
            const auto& error = std::string(infoLog.data());
            TBX_ASSERT(false, "GL Rendering: Shader compilation failure: {}", error);
            return;
        }

        // Attach our shader to our program
        glAttachShader(_programId, _shaderId);
    }

    void OpenGLShader::UploadUniform(const Tbx::ShaderUniform& data) const
    {
        if (std::holds_alternative<Tbx::Mat4x4>(data.Data))
        {
            UploadUniformMat4(data.Name, std::get<Tbx::Mat4x4>(data.Data), _programId);
        }
        else if (std::holds_alternative<Tbx::Vector2>(data.Data))
        {
            UploadUniformFloat2(data.Name, std::get<Tbx::Vector2>(data.Data), _programId);
        }
        else if (std::holds_alternative<Tbx::Vector3>(data.Data))
        {
            UploadUniformFloat3(data.Name, std::get<Tbx::Vector3>(data.Data), _programId);
        }
        else if (std::holds_alternative<Tbx::RgbaColor>(data.Data))
        {
            UploadUniformFloat4(data.Name, std::get<Tbx::RgbaColor>(data.Data), _programId);
        }
        else if (std::holds_alternative<float>(data.Data))
        {
            UploadUniformFloat(data.Name, std::get<float>(data.Data), _programId);
        }
        else if (std::holds_alternative<int>(data.Data))
        {
            UploadUniformInt(data.Name, std::get<int>(data.Data), _programId);
        }
        else
        {
            TBX_ASSERT(false, "GL Rendering: Unsupported shader data type.");
        }
    }

    void OpenGLShader::Detach() const
    {
        glDetachShader(_programId, _shaderId);
    }

    ///// Material //////////////////////////////////////////////////////////////////

    OpenGLMaterial::~OpenGLMaterial()
    {
        glUseProgram(0);
        glDeleteProgram(_materialGLId);
    }

    void OpenGLMaterial::Upload(const Tbx::Material& material)
    {
        _materialGLId = glCreateProgram();

        {
            // Compile shaders
            for (const auto& shader : material.Shaders)
            {
                auto& glShader = _shaders.emplace_back();
                glShader.Compile(*shader, _materialGLId);
            }

            // Link our program
            glLinkProgram(_materialGLId);

            // Note the different functions here: glGetProgram* instead of glGetShader*.
            GLint isLinked = 0;
            glGetProgramiv(_materialGLId, GL_LINK_STATUS, &isLinked);
            if (isLinked == GL_FALSE) // Check if we failed linking
            {
                // Get the error
                GLint maxLength = 0;
                glGetProgramiv(_materialGLId, GL_INFO_LOG_LENGTH, &maxLength);
                std::vector<GLchar> infoLog(maxLength);
                glGetProgramInfoLog(_materialGLId, maxLength, &maxLength, &infoLog[0]);

                // Cleanup
                glDeleteProgram(_materialGLId);
                _shaders.clear();

                // Assert
                const auto& error = std::string(infoLog.data());
                TBX_ASSERT(false, "GL Rendering: Shader link failure: {0}", error);
                return;
            }

            // Detach after successful link
            for (const auto& shader : _shaders)
            {
                shader.Detach();
            }
        }
    }

    void OpenGLMaterial::Bind() const
    {
        glUseProgram(_materialGLId);
    }

    void OpenGLMaterial::Unbind() const
    {
        glUseProgram(0);
    }

    void OpenGLMaterial::UploadUniform(const Tbx::ShaderUniform& data) const
    {
        for (const auto& shader : _shaders)
        {
            shader.UploadUniform(data);
        }
    }

    ///// Material Instance //////////////////////////////////////////////////////////////////

    void OpenGLMaterialInstance::Upload(const Tbx::MaterialInstance& material)
    {
        _material.Bind();
        Tbx::uint slot = 0;
        for (const auto& texture : material.Textures)
        {
            auto& glTexture = _textures.emplace_back();
            glTexture.Upload(*texture, slot);
            slot++;
        }
        _material.Unbind();
    }

    void OpenGLMaterialInstance::Bind() const
    {
        _material.Bind();
        for (const auto& tex : _textures)
        {
            tex.Bind();
        }
    }

    void OpenGLMaterialInstance::Unbind() const
    {
        _material.Unbind();
        for (const auto& tex : _textures)
        {
            tex.Unbind();
        }
    }

    void OpenGLMaterialInstance::SetUniform(const Tbx::ShaderUniform& data) const
    {
        _material.Bind();
        _material.UploadUniform(data);
    }
}
