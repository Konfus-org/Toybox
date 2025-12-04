#include "OpenGLShader.h"
#include <Tbx/Math/Vectors.h>
#include <Tbx/Math/Mat4x4.h>
#include <Tbx/Graphics/Color.h>
#include <Tbx/Debug/Asserts.h>
#include <glad/glad.h>
#include <vector>

namespace Tbx::Plugins::OpenGLRendering
{
    ///// Helpers //////////////////////////////////////////////////////////////////
    static uint GetUniformLocation(const std::string name, uint programId)
    {
        GLint location = glGetUniformLocation(programId, name.c_str());
        //TBX_ASSERT(location != -1, "Invalid uniform location: {}", name);
        return location;
    }

    static void UploadUniformInt(const std::string& name, int value, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1i(location, value);
    }

    static void UploadUniformIntArray(const std::string& name, std::vector<int> values, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1iv(location, (uint32)values.size(), values.data());
    }

    static void UploadUniformFloat(const std::string& name, float value, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform1f(location, value);
    }

    static void UploadUniformFloat2(const std::string& name, const Vector2& value, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform2f(location, value.X, value.Y);
    }

    static void UploadUniformFloat3(const std::string& name, const Vector3& value, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform3f(location, value.X, value.Y, value.Z);
    }

    static void UploadUniformFloat4(const std::string& name, const RgbaColor& value, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniform4f(location, value.R, value.G, value.B, value.A);
    }

    // TODO: Implement when needed... Toybox doesn't have a mat3 yet...
    ////static void UploadUniformMat3(const std::string& name, const Matrix& matrix, uint rendererId)
    ////{
    ////	GLint location = glGetUniformLocation(rendererId, name.c_str());
    ////	glUniformMatrix3fv(location, 1, GL_FALSE, matrix.Values.data());
    ////}

    static void UploadUniformMat4(const std::string& name, const Mat4x4& matrix, uint programId)
    {
        GLint location = GetUniformLocation(name.c_str(), programId);
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix.Values.data());
    }

    OpenGLShader::OpenGLShader(const Shader& shader)
    {
        _type = shader.Type;

        // Create a shader handle
        if (shader.Type == ShaderType::Vertex)
        {
            RenderId = glCreateShader(GL_VERTEX_SHADER);
        }
        else if (shader.Type == ShaderType::Fragment)
        {
            RenderId = glCreateShader(GL_FRAGMENT_SHADER);
        }
        else
        {
            TBX_ASSERT(false, "GL Rendering: Unsupported shader type.");
        }

        // Send the shader source code to GL
        // Note that std::string's .c_str is NULL character terminated.
        const auto* source = shader.Source.c_str();
        glShaderSource(RenderId, 1, &source, nullptr);

        // Compile the vertex shader
        glCompileShader(RenderId);

        GLint isCompiled = 0;
        glGetShaderiv(RenderId, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) // Check if we failed compilation
        {
            // Get the error
            GLint maxLength = 0;
            glGetShaderiv(RenderId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(RenderId, maxLength, &maxLength, &infoLog[0]);

            // Cleanup
            glDeleteShader(RenderId);

            // Assert
            const auto& error = std::string(infoLog.data());
            TBX_ASSERT(false, "GL Rendering: Shader compilation failure: {}", error);
            return;
        }
    }

    ///// Shader //////////////////////////////////////////////////////////////////
    OpenGLShader::~OpenGLShader()
    {
        glDeleteShader(RenderId);
    }

    ///// ShaderProgram //////////////////////////////////////////////////////////////////

    OpenGLShaderProgram::OpenGLShaderProgram(const std::vector<Ref<ShaderResource>>& shaders)
    {
        // Create program
        RenderId = glCreateProgram();

        // Attach shaders
        for (const auto& shader : shaders)
        {
            glAttachShader(RenderId, shader->RenderId);
        }

        // Link our program
        glLinkProgram(RenderId);
        GLint isLinked = 0;
        glGetProgramiv(RenderId, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE) // Check if we failed linking
        {
            // Get the error
            GLint maxLength = 0;
            glGetProgramiv(RenderId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(RenderId, maxLength, &maxLength, &infoLog[0]);

            // Cleanup
            glDeleteProgram(RenderId);

            // Assert
            const auto& error = std::string(infoLog.data());
            TBX_ASSERT(false, "GL Rendering: Shader link failure: {0}", error);
            return;
        }

        // Detach after successful link
        for (const auto& shader : shaders)
        {
            glDetachShader(RenderId, shader->RenderId);
        }
    }

    OpenGLShaderProgram::~OpenGLShaderProgram()
    {
        glDeleteProgram(RenderId);
    }

    void OpenGLRendering::OpenGLShaderProgram::Activate()
    {
        glUseProgram(RenderId);
    }

    void OpenGLRendering::OpenGLShaderProgram::Release()
    {
        glUseProgram(0);
    }

    void OpenGLRendering::OpenGLShaderProgram::Upload(const ShaderUniform& uniform)
    {
        if (std::holds_alternative<Mat4x4>(uniform.Data))
        {
            UploadUniformMat4(uniform.Name, std::get<Mat4x4>(uniform.Data), RenderId);
        }
        else if (std::holds_alternative<Vector2>(uniform.Data))
        {
            UploadUniformFloat2(uniform.Name, std::get<Vector2>(uniform.Data), RenderId);
        }
        else if (std::holds_alternative<Vector3>(uniform.Data))
        {
            UploadUniformFloat3(uniform.Name, std::get<Vector3>(uniform.Data), RenderId);
        }
        else if (std::holds_alternative<RgbaColor>(uniform.Data))
        {
            UploadUniformFloat4(uniform.Name, std::get<RgbaColor>(uniform.Data), RenderId);
        }
        else if (std::holds_alternative<float>(uniform.Data))
        {
            UploadUniformFloat(uniform.Name, std::get<float>(uniform.Data), RenderId);
        }
        else if (std::holds_alternative<int>(uniform.Data))
        {
            UploadUniformInt(uniform.Name, std::get<int>(uniform.Data), RenderId);
        }
        else
        {
            TBX_ASSERT(false, "GL Rendering: Unsupported shader data type.");
        }
    }
}
