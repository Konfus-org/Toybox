#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/shader.generated.h"
#include "tbx/types/handle.h"
#include "tbx/utils/hash.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Enumerates the supported shader stage types.
    /// @details
    /// Ownership: Does not own resources.
    /// Thread Safety: Safe to read concurrently.
    [[serializable]];
    enum class ShaderType
    {
        NONE [[name("none")]],
        VERTEX [[name("vertex")]],
        TESSELATION [[name("tesselation")]],
        GEOMETRY [[name("geometry")]],
        FRAGMENT [[name("fragment")]],
        COMPUTE [[name("compute")]]
    };

    /// @brief
    /// Purpose: Stores shader source text for a single stage.
    /// @details
    /// Ownership: Owns the source string data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    [[serializable("text")]];
    [[version(1U)]];
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

        std::string source = "";

        [[meta]]
        ShaderType type = ShaderType::NONE;
    };

    /// @brief
    /// Purpose: Holds explicit shader stage handles used to build a shader program.
    /// @details
    /// Ownership: Stores stage handles by value; does not own loaded shader assets.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    struct TBX_API ShaderProgram
    {
      public:
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
        std::vector<Handle> computes = {};

        /// @brief
        /// Purpose: Returns whether any stage handle is set.
        /// @details
        /// Ownership: Stateless; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
        bool is_valid() const
        {
            const bool has_compute = !computes.empty();
            const bool has_graphics_stages = vertex.id.is_valid() || fragment.id.is_valid()
                                             || tesselation.id.is_valid() || geometry.id.is_valid();

            if (has_compute)
                return !has_graphics_stages;

            if (!vertex.id.is_valid() || !fragment.id.is_valid())
                return false;

            return true;
        }
    };

    /// @brief Folds a shader program's stage handles into a stable hash. Combined with the render
    /// state by the material-level hash overload to key a compiled raster pipeline.
    inline uint64 hash(const ShaderProgram& shader)
    {
        uint64 value = hash_handle(shader.vertex.id);
        value = hash_combine(value, hash_handle(shader.fragment.id));
        value = hash_combine(value, hash_handle(shader.geometry.id));
        value = hash_combine(value, hash_handle(shader.tesselation.id));
        return value;
    }

    /// @brief
    /// Purpose: Provides shader-specific read parameters for serialized shader assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct ShaderLoadParameters
    {
        bool operator==(const ShaderLoadParameters& other) const = default;
    };

    ShaderLoadParameters load_parameters_of(const Shader&);
}
