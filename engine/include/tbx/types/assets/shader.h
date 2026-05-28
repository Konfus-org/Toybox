#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vectors.h"
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
    [[tbx::serializable]];
    enum class ShaderType
    {
        NONE [[tbx::name("none")]],
        VERTEX [[tbx::name("vertex")]],
        TESSELATION [[tbx::name("tesselation")]],
        GEOMETRY [[tbx::name("geometry")]],
        FRAGMENT [[tbx::name("fragment")]],
        COMPUTE [[tbx::name("compute")]]
    };

    /// @brief
    /// Purpose: Stores shader source text for a single stage.
    /// @details
    /// Ownership: Owns the source string data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    [[tbx::serializable]];
    [[tbx::version(1U)]];
    struct TBX_API ShaderSource
    {
        ShaderSource() = default;
        ShaderSource(const char* shader_source, ShaderType shader_type)
            : source(shader_source ? shader_source : "")
            , type(shader_type)
        {
        }
        ShaderSource(std::string_view shader_source, ShaderType shader_type)
            : source(shader_source)
            , type(shader_type)
        {
        }
        ShaderSource(std::string&& shader_source, ShaderType shader_type)
            : source(std::move(shader_source))
            , type(shader_type)
        {
        }

        // TODO: Update serializable attribute to have a optional 'mode' param that defaults to
        // "json", the other option is "text" the text option expects one prop that is a string or
        // char* or string view to serialize into plain text if there is more than one prop we
        // should get a compiler error!
        [[tbx::text]]
        std::string source = "";

        [[tbx::meta]]
        ShaderType type = ShaderType::NONE;
    };

    /// @brief
    /// Purpose: Stores one or more shader stages that can be linked into a graphics pipeline.
    /// @details
    /// Ownership: Owns copied shader stage sources.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API ShaderProgram : Asset
    {
        ShaderProgram() = default;
        ShaderProgram(const char* shader_source, ShaderType shader_type)
            : sources({ShaderSource(shader_source, shader_type)})
        {
        }
        ShaderProgram(std::string_view shader_source, ShaderType shader_type)
            : sources({ShaderSource(shader_source, shader_type)})
        {
        }
        ShaderProgram(std::string&& shader_source, ShaderType shader_type)
            : sources({ShaderSource(std::move(shader_source), shader_type)})
        {
        }
        ShaderProgram(ShaderSource shader_source)
            : sources({std::move(shader_source)})
        {
        }
        ShaderProgram(std::vector<ShaderSource> shader_sources)
            : sources(std::move(shader_sources))
        {
        }

        bool is_valid() const
        {
            bool has_vertex = false;
            bool has_fragment = false;
            bool has_tesselation = false;
            bool has_geometry = false;
            bool has_compute = false;

            for (const auto& source : sources)
            {
                switch (source.type)
                {
                    case ShaderType::VERTEX:
                        if (has_vertex)
                            return false;
                        has_vertex = true;
                        break;
                    case ShaderType::FRAGMENT:
                        if (has_fragment)
                            return false;
                        has_fragment = true;
                        break;
                    case ShaderType::TESSELATION:
                        if (has_tesselation)
                            return false;
                        has_tesselation = true;
                        break;
                    case ShaderType::GEOMETRY:
                        if (has_geometry)
                            return false;
                        has_geometry = true;
                        break;
                    case ShaderType::COMPUTE:
                        if (has_compute)
                            return false;
                        has_compute = true;
                        break;
                    case ShaderType::NONE:
                    default:
                        return false;
                }
            }

            if (has_compute)
                return !has_vertex && !has_fragment && !has_tesselation && !has_geometry;

            return has_vertex && has_fragment;
        }

        std::string source = "";
        ShaderType type = ShaderType::NONE;
        std::vector<ShaderSource> sources = {};
    };

    /// @brief
    /// Purpose: Holds explicit shader stage handles used to build a shader program.
    /// @details
    /// Ownership: Stores stage handles by value; does not own loaded shader assets.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
    struct TBX_API Shader
    {
        /// @brief
        /// Purpose: Identifies the vertex shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        [[tbx::prop]]
        Handle vertex = {};

        /// @brief
        /// Purpose: Identifies the fragment shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        [[tbx::prop]]
        Handle fragment = {};

        /// @brief
        /// Purpose: Identifies the optional tessellation control and evaluation shader stages.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        [[tbx::prop]]
        Handle tesselation = {};

        /// @brief
        /// Purpose: Identifies the geometry shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        [[tbx::prop]]
        Handle geometry = {};

        /// @brief
        /// Purpose: Identifies the compute shader stage asset.
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        [[tbx::prop]]
        Handle compute = {};

        /// @brief
        /// Purpose: Returns whether any stage handle is set.
        /// @details
        /// Ownership: Stateless; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
        bool is_valid() const
        {
            const bool has_compute = compute.id.is_valid();
            const bool has_graphics_stages = vertex.id.is_valid() || fragment.id.is_valid()
                                             || tesselation.id.is_valid() || geometry.id.is_valid();

            if (has_compute)
                return !has_graphics_stages;

            if (!vertex.id.is_valid() || !fragment.id.is_valid())
                return false;

            return true;
        }
    };

}

#include "tbx/types/assets/shader.generated.h"
