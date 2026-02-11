#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents the supported values for material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns any stored value data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    using UniformData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, RgbaColor, Mat3, Mat4>;

    /// <summary>
    /// Purpose: Represents a uniform upload payload for shader programs.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores owned copies of uniform data values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API ShaderUniform
    {
        std::string name = "";
        UniformData data = 0;
    };

    /// <summary>
    /// Purpose: Enumerates the supported shader stage types.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own resources.
    /// Thread Safety: Safe to read concurrently.
    /// </remarks>
    enum class ShaderType
    {
        NONE,
        VERTEX,
        TESSELATION,
        GEOMETRY,
        FRAGMENT,
        COMPUTE
    };

    /// <summary>
    /// Purpose: Stores shader source text for a single stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the source string data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API ShaderSource
    {
        ShaderSource() = default;
        ShaderSource(std::string shader_source, ShaderType shader_type)
            : source(std::move(shader_source))
            , type(shader_type)
        {
        }

        std::string source = "";
        ShaderType type = ShaderType::NONE;
    };

    /// <summary>
    /// Purpose: Aggregates shader sources into a program description.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the shader source collection.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Shader
    {
        Shader() = default;
        explicit Shader(ShaderSource shader_source)
            : sources({std::move(shader_source)})
        {
        }
        explicit Shader(std::vector<ShaderSource> shader_sources)
            : sources(std::move(shader_sources))
        {
        }

        std::vector<ShaderSource> sources = {};
    };
}
