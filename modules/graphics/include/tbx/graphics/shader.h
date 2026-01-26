#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <string>
#include <variant>

namespace tbx
{
    using UniformData = std::variant<bool, int, float, Vec2, Vec3, RgbaColor, Mat4>;

    // A uniform variable that can be uploaded to a shader.
    struct TBX_API ShaderUniform
    {
        std::string name = "";
        UniformData data = 0;
    };

    // The type of a shader.
    enum class TBX_API ShaderType
    {
        None,
        Vertex,
        Fragment,
        Compute
    };

    // A shader is a program that runs on the GPU and is responsible for rendering.
    // It consists of a source code and a type.
    struct TBX_API Shader
    {
        Shader() = default;
        Shader(const std::string& source, ShaderType type)
            : source(source)
            , type(type)
        {
        }

        std::string source = "";
        ShaderType type = ShaderType::None;
        Uuid id = Uuid::generate();

        /// <summary>Provides the default vertex shader definition.</summary>
        /// <remarks>Purpose: Supplies a shared default vertex shader template.
        /// Ownership: Returns a reference to a static Shader owned by the graphics module.
        /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
        static TBX_API const Shader& default_vert();

        /// <summary>Provides the default fragment shader definition.</summary>
        /// <remarks>Purpose: Supplies a shared default fragment shader template.
        /// Ownership: Returns a reference to a static Shader owned by the graphics module.
        /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
        static TBX_API const Shader& default_frag();
    };

    // Compiles a shader.
    class TBX_API IShaderCompiler
    {
      public:
        virtual ~IShaderCompiler() noexcept = default;

        // Compiles a shader.
        // Returns true on success and false on failure.
        virtual bool compile(std::shared_ptr<Shader> shader) = 0;
    };
}
