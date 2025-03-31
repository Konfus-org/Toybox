#include "OpenGLShader.h"
#include <Tbx/Core/Rendering/Color.h>

namespace OpenGLRendering
{
	///// Helpers //////////////////////////////////////////////////////////////////
	static void UploadUniformInt(const std::string& name, int value, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform1i(location, value);
	}

	static void UploadUniformIntArray(const std::string& name, std::vector<int> values, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform1iv(location, (Tbx::uint32)values.size(), values.data());
	}

	static void UploadUniformFloat(const std::string& name, float value, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform1f(location, value);
	}

	static void UploadUniformFloat2(const std::string& name, const Tbx::Vector2& value, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform2f(location, value.X, value.Y);
	}

	static void UploadUniformFloat3(const std::string& name, const Tbx::Vector3& value, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform3f(location, value.X, value.Y, value.Z);
	}

	static void UploadUniformFloat4(const std::string& name, const Tbx::Color& value, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniform4f(location, value.R, value.G, value.B, value.A);
	}

	// TODO: Implement when needed... Toybox doesn't have a mat3 yet...
	////static void UploadUniformMat3(const std::string& name, const Tbx::Matrix& matrix, Tbx::uint rendererId)
	////{
	////	GLint location = glGetUniformLocation(rendererId, name.c_str());
	////	glUniformMatrix3fv(location, 1, GL_FALSE, matrix.Values.data());
	////}

	static void UploadUniformMat4(const std::string& name, const Tbx::Matrix& matrix, Tbx::uint rendererId)
	{
		GLint location = glGetUniformLocation(rendererId, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, matrix.Values.data());
	}

	///// Shader Class //////////////////////////////////////////////////////////////////
	OpenGLShader::~OpenGLShader()
	{
		glUseProgram(0);
		glDeleteProgram(_rendererId);
	}

	void OpenGLShader::Compile(const Tbx::Shader& shader)
	{
		// The code below is a lighty modified version of the example code found here:
		// https://www.khronos.org/opengl/wiki/Shader_Compilation
		
		_associatedAssetId = shader.GetId();

        const auto& vertexSrc = shader.GetVertexSource();
        const auto& fragmentSrc = shader.GetFragmentSource();

		// Create an empty vertex shader handle
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const auto* source = vertexSrc.c_str();
		glShaderSource(vertexShader, 1, &source, nullptr);

		// Compile the vertex shader
		glCompileShader(vertexShader);

		GLint isCompiled = 0;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(vertexShader);

			const auto& error = std::string(infoLog.data());
			TBX_ASSERT(false, "Vertex shader compilation failure: {0}", error);

			// In this simple program, we'll just leave
			return;
		}

		// Create an empty fragment shader handle
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

		// Send the fragment shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		source = fragmentSrc.c_str();
		glShaderSource(fragmentShader, 1, &source, nullptr);

		// Compile the fragment shader
		glCompileShader(fragmentShader);

		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(fragmentShader);
			// Either of them. Don't leak shaders.
			glDeleteShader(vertexShader);

			const auto& error = std::string(infoLog.data());
			TBX_ASSERT(false, "Fragment shader compilation failure: {0}", error);

			// In this simple program, we'll just leave
			return;
		}

		// Vertex and fragment shaders are successfully compiled.
		// Now time to link them together into a program.
		// Get a program object.
		_rendererId = glCreateProgram();

		// Attach our shaders to our program
		glAttachShader(_rendererId, vertexShader);
		glAttachShader(_rendererId, fragmentShader);

		// Link our program
		glLinkProgram(_rendererId);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
		glGetProgramiv(_rendererId, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(_rendererId, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(_rendererId, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(_rendererId);
			// Don't leak shaders either.
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);

			const auto& error = std::string(infoLog.data());
			TBX_ASSERT(false, "Shader link failure: {0}", error);

			// In this simple program, we'll just leave
			return;
		}

		// Always detach shaders after a successful link.
		glDetachShader(_rendererId, vertexShader);
		glDetachShader(_rendererId, fragmentShader);
	}

	void OpenGLShader::Bind() const
	{
		glUseProgram(_rendererId);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::UploadData(const Tbx::ShaderData& data) const
	{
		switch (data.GetType())
		{
			case Tbx::ShaderDataType::Mat4:
			{
                UploadUniformMat4(data.GetName(), std::any_cast<Tbx::Matrix>(data.GetData()), _rendererId);
				break;
			}
            case Tbx::ShaderDataType::Float:
            {
                UploadUniformFloat(data.GetName(), std::any_cast<float>(data.GetData()), _rendererId);
                break;
            }
            case Tbx::ShaderDataType::Float2:
            {
                UploadUniformFloat2(data.GetName(), std::any_cast<Tbx::Vector2>(data.GetData()), _rendererId);
                break;
            }
            case Tbx::ShaderDataType::Float3:
            {
                UploadUniformFloat3(data.GetName(), std::any_cast<Tbx::Vector3>(data.GetData()), _rendererId);
                break;
            }
            case Tbx::ShaderDataType::Float4:
            {
                UploadUniformFloat4(data.GetName(), std::any_cast<Tbx::Color>(data.GetData()), _rendererId);
                break;
            }
            case Tbx::ShaderDataType::Int:
            {
				UploadUniformInt(data.GetName(), std::any_cast<int>(data.GetData()), _rendererId);
                break;
            }
			default:
			{
				TBX_ASSERT(false, "Unsupported shader data type.");
				break;
			}
		}
	}
}
