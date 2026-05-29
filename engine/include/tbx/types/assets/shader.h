#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/handle.h"
#include <string>
#include <string_view>
#include <utility>

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
    [[tbx::serializable("text")]];
    [[tbx::version(1U)]];
    struct TBX_API Shader : public Asset
    {
        Shader() = default;
        Shader(const char* shader_source, ShaderType shader_type)
            : source(shader_source ? shader_source : "")
            , type(shader_type)
        {
        }
        Shader(std::string_view shader_source, ShaderType shader_type)
            : source(shader_source)
            , type(shader_type)
        {
        }
        Shader(std::string&& shader_source, ShaderType shader_type)
            : source(std::move(shader_source))
            , type(shader_type)
        {
        }

        [[tbx::prop]]
        std::string source = "";

        [[tbx::meta]]
        ShaderType type = ShaderType::NONE;
    };

    /// @brief
    /// Purpose: Holds explicit shader stage handles used to build a shader program.
    /// @details
    /// Ownership: Stores stage handles by value; does not own loaded shader assets.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
    struct TBX_API ShaderProgram
    {
      public:
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
