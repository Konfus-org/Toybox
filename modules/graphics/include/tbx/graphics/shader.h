#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
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

    /// <summary>
    /// Purpose: Holds explicit shader stage handles used to build a shader program.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores stage handles by value; does not own loaded shader assets.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API ShaderProgram
    {
        /// <summary>
        /// Purpose: Identifies the vertex shader stage asset.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle vertex = {};

        /// <summary>
        /// Purpose: Identifies the fragment shader stage asset.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle fragment = {};

        /// <summary>
        /// Purpose: Identifies the optional tessellation control and evaluation shader stages.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle tesselation = {};

        /// <summary>
        /// Purpose: Identifies the geometry shader stage asset.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle geometry = {};

        /// <summary>
        /// Purpose: Identifies the compute shader stage asset.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle compute = {};

        /// <summary>
        /// Purpose: Returns whether any stage handle is set.
        /// </summary>
        /// <remarks>
        /// Ownership: Stateless; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool is_valid() const
        {
            const bool has_compute = compute.is_valid();
            const bool has_graphics_stages = vertex.is_valid() || fragment.is_valid()
                                             || tesselation.is_valid() || geometry.is_valid();

            if (has_compute)
                return !has_graphics_stages;

            if (!vertex.is_valid() || !fragment.is_valid())
                return false;

            return true;
        }
    };
}
