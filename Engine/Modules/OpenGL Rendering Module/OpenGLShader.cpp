#include "OpenGLShader.h"
#include <glad/glad.h>
#include <Core.h>

OpenGLShader::~OpenGLShader()
{
	glDeleteProgram(_rendererId);
}

void OpenGLShader::Compile(const std::string& vertexSrc, const std::string& fragmentSrc)
{
	// The code below is a lighty modified version of the example code found here:
	// https://www.khronos.org/opengl/wiki/Shader_Compilation

	// Unbind from any old shader src we were using if we re-compile
	Unbind();
	glDeleteProgram(_rendererId);
	
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
