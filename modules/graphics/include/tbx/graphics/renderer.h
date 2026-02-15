#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/post_processing.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines a model handle to use within a specific distance band (LOD).
    /// </summary>
    /// <remarks>
    /// Ownership: Stores handles by value; does not own loaded model assets.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API RendererLod
    {
        /// <summary>
        /// Purpose: Model asset handle used when within the specified distance band.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle handle = {};

        /// <summary>
        /// Purpose: Inclusive maximum distance (world units) where this LOD should be used.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float max_distance = 0.0f;
    };

    /// <summary>
    /// Purpose: Stores render settings and material selection for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores handles and override data by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Renderer
    {
        /// <summary>
        /// Purpose: Runtime material data for this entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns parameter/texture override sets and a base material handle.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        MaterialInstance material = {};

        /// <summary>
        /// Purpose: Enables or disables culling behavior for this entity (e.g., render distance
        /// cull).
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_cullable = true;

        /// <summary>
        /// Purpose: Enables or disables shadow participation for this entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool are_shadows_enabled = true;

        /// <summary>
        /// Purpose: Marks the surface as two-sided for rendering.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_two_sided = false;

        /// <summary>
        /// Purpose: Limits rendering based on distance from the active camera (0 means unlimited).
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float render_distance = 0.0f;

        /// <summary>
        /// Purpose: Provides optional LOD model handles selected based on camera distance.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the LOD vector.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        std::vector<RendererLod> lods = {};
    };

}
