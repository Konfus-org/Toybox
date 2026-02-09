#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/shader.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines a mutable collection of material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns its parameter collections.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Material
    {
        /// <summary>
        /// Purpose: Identifies the shader assets used to render the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the shader handles by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        std::vector<Handle> shaders = {};

        /// <summary>
        /// Purpose: Stores named texture asset bindings for the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the texture name strings and handle values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<std::pair<std::string, Handle>> textures = {};

        /// <summary>
        /// Purpose: Stores material parameters.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the parameter list and associated values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<std::pair<std::string, UniformData>> parameters = {};
    };
}
