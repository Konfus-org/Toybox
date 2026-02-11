#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/shader.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
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
        /// Purpose: Provides explicit shader stage handles used to build a single shader program.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores stage handles by value; does not own loaded shader assets.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        ShaderProgram shader = {};

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
