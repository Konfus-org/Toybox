#pragma once
#include "tbx/systems/graphics/color.h"
#include "tbx/systems/math/matrices.h"
#include "tbx/systems/math/vectors.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>


namespace tbx
{
    /// @brief
    /// Purpose: Enumerates the supported shader stage types.
    /// @details
    /// Ownership: Does not own resources.
    /// Thread Safety: Safe to read concurrently.
    enum class ShaderType
    {
        NONE,
        VERTEX,
        TESSELATION,
        GEOMETRY,
        FRAGMENT,
        COMPUTE
    };

    /// @brief
    /// Purpose: Stores shader source text for a single stage.
    /// @details
    /// Ownership: Owns the source string data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
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

    /// @brief
    /// Purpose: Aggregates shader sources into a program description.
    /// @details
    /// Ownership: Owns the shader source collection.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API Shader
    {
        Shader() = default;
        Shader(ShaderSource shader_source)
            : sources({std::move(shader_source)})
        {
        }
        Shader(std::vector<ShaderSource> shader_sources)
            : sources(std::move(shader_sources))
        {
        }

        std::vector<ShaderSource> sources = {};
    };

    /// @brief
    /// Purpose: Holds explicit shader stage handles used to build a shader program.
    /// @details
    /// Ownership: Stores stage handles by value; does not own loaded shader assets.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API ShaderProgram
    {
        /// @brief
        /// Purpose: Identifies the vertex shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle vertex = {};

        /// @brief
        /// Purpose: Identifies the fragment shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle fragment = {};

        /// @brief
        /// Purpose: Identifies the optional tessellation control and evaluation shader stages.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle tesselation = {};

        /// @brief
        /// Purpose: Identifies the geometry shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle geometry = {};

        /// @brief
        /// Purpose: Identifies the compute shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle compute = {};

        /// @brief
        /// Purpose: Returns whether any stage handle is set.
        /// @details
        /// Ownership: Stateless; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
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
