#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Mat4x4.h"
#include <string>
#include <variant>

namespace Tbx
{
    using UniformData = std::variant<bool, int, float, Vector2, Vector3, RgbaColor, Mat4x4>;

    struct TBX_EXPORT ShaderUniform
    {
        std::string Name = "";
        UniformData Data = 0;
    };

    /// <summary>
    /// The type of a shader.
    /// </summary>
    enum class TBX_EXPORT ShaderLang
    {
        None,
        GLSL,
        HLSL,
        Custom
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

    struct TBX_EXPORT Shader
    {
        std::string Source = "";
        ShaderType Type = ShaderType::None;
        ShaderLang Lang = ShaderLang::None;
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

        /// <summary>
        /// Gets the supported shader languages.
        /// </summary>
        /// <returns></returns>
        virtual std::vector<ShaderLang> GetSupportedLanguages() const = 0;
    };
}