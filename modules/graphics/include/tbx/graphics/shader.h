#pragma once
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"
#include <string>
#include <variant>

namespace tbx
{
    using UniformData = std::variant<bool, int, float, Vec2, Vec3, RgbaColor, Mat4>;

    /// <summary>
    /// A uniform variable that can be uploaded to a shader.
    /// </summary>
    struct TBX_API ShaderUniform
    {
        std::string name = "";
        UniformData data = 0;
    };

    /// <summary>
    /// The type of a shader.
    /// </summary>
    enum class TBX_API ShaderType
    {
        None,
        Vertex,
        Fragment,
        Compute
    };

    /// <summary>
    /// A shader is a program that runs on the GPU and is responsible for rendering.
    /// It consists of a source code and a type.
    /// </summary>
    struct TBX_API Shader
    {
        Shader() = default;
        Shader(const std::string& source, ShaderType type)
            : source(source), type(type) {}

        std::string source = "";
        ShaderType type = ShaderType::None;
        uuid id = uuid::generate();
    };

    /// <summary>
    /// Compiles a shader.
    /// </summary>
    class TBX_API IShaderCompiler
    {
    public:
        virtual ~IShaderCompiler() = default;

        /// <summary>
        /// Compiles a shader.
        /// Returns true on success and false on failure.
        /// </summary>
        virtual bool compile(Ref<Shader> shader) = 0;
    };
}
