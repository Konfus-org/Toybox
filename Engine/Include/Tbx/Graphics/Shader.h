#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"
#include <string>
#include <variant>

namespace Tbx
{
    using UniformData = std::variant<bool, int, float, Vector2, Vector3, RgbaColor, Mat4x4>;

    /// <summary>
    /// A uniform variable that can be uploaded to a shader.
    /// </summary>
    struct TBX_EXPORT ShaderUniform
    {
        std::string Name = "";
        UniformData Data = 0;
    };

    /// <summary>
    /// The type of a shader.
    /// </summary>
    enum class TBX_EXPORT ShaderType
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
    struct TBX_EXPORT Shader
    {
        std::string Source = "";
        ShaderType Type = ShaderType::None;
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// Compiles a shader.
    /// </summary>
    class TBX_EXPORT IShaderCompiler
    {
    public:
        virtual ~IShaderCompiler() = default;

        /// <summary>
        /// Compiles a shader.
        /// Returns true on success and false on failure.
        /// </summary>
        virtual bool Compile(Ref<Shader> shader) = 0;
    };
}