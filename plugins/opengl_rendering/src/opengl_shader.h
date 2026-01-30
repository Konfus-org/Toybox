#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/shader.h"
#include <memory>
#include <vector>

namespace tbx::plugins
{
    /// <summary>OpenGL implementation of a shader stage resource.</summary>
    /// <remarks>Purpose: Compiles shader source into an OpenGL shader object.
    /// Ownership: Owns the OpenGL shader identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlShader final : public IGlResource
    {
      public:
        /// <summary>Creates an OpenGL shader from source.</summary>
        /// <remarks>Purpose: Compiles the shader source into a GPU shader object.
        /// Ownership: Owns the created shader identifier.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlShader(const ShaderSource& shader);

        /// <summary>Destroys the OpenGL shader resource.</summary>
        /// <remarks>Purpose: Releases the GPU shader identifier.
        /// Ownership: Owns the GPU handle being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlShader() noexcept override;

        /// <summary>Returns the shader stage type.</summary>
        /// <remarks>Purpose: Allows inspection of the shader stage.
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        ShaderType get_type() const;

        /// <summary>Binds the shader stage.</summary>
        /// <remarks>Purpose: OpenGL stages are activated via programs; this is a no-op.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the shader stage.</summary>
        /// <remarks>Purpose: OpenGL stages are activated via programs; this is a no-op.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        void unbind() override;

        /// <summary>Returns the OpenGL shader handle.</summary>
        /// <remarks>Purpose: Used internally when linking programs.
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        uint32 get_shader_id() const;

      private:
        uint32 _shader_id = 0;
        ShaderType _type = ShaderType::None;
    };

    /// <summary>OpenGL implementation of a shader program resource.</summary>
    /// <remarks>Purpose: Links shader stages into a GPU program.
    /// Ownership: Owns the OpenGL program identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlShaderProgram final : public IGlResource
    {
      public:
        /// <summary>Creates and links a shader program.</summary>
        /// <remarks>Purpose: Links provided shader stages into a program.
        /// Ownership: Owns the created program identifier.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlShaderProgram(
            const std::vector<std::shared_ptr<OpenGlShader>>& shaders);

        /// <summary>Destroys the shader program.</summary>
        /// <remarks>Purpose: Releases the GPU program identifier.
        /// Ownership: Owns the GPU handle being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlShaderProgram() noexcept override;

        /// <summary>Binds the program for rendering.</summary>
        /// <remarks>Purpose: Binds the program so subsequent draw calls use it.
        /// Ownership: The program retains ownership of its GPU handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the program.</summary>
        /// <remarks>Purpose: Unbinds the program from the pipeline.
        /// Ownership: The program retains ownership of its GPU handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

        /// <summary>Uploads a uniform value to the program.</summary>
        /// <remarks>Purpose: Updates uniform state used by the program.
        /// Ownership: Copies uniform data; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(const ShaderUniform& uniform);

      private:
        uint32 _program_id = 0;
    };
}
