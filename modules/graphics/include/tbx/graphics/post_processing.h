#pragma once
#include "tbx/graphics/material.h"
#include "tbx/tbx_api.h"
#include <initializer_list>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Describes one material-driven post-processing step in the effect stack.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning material handle reference and value settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API PostProcessingEffect
    {
        /// <summary>
        /// Purpose: Runtime material data used to shade this fullscreen post-processing pass.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns parameter/texture override sets and a base material handle.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        MaterialInstance material = {};

        /// <summary>
        /// Purpose: Enables or disables this effect without removing it from the stack.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_enabled = true;

        /// <summary>
        /// Purpose: Controls blend weight between source scene color (0) and effect output (1).
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float blend = 1.0f;
    };

    /// <summary>
    /// Purpose: Configures the scene-wide post-processing material for the final screen pass.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value settings and an ordered effect stack by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API PostProcessing
    {
        /// <summary>
        /// Purpose: Ordered list of effects to apply from first to last.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the effect stack vector and effect settings.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        std::vector<PostProcessingEffect> effects = {};

        /// <summary>
        /// Purpose: Enables or disables post-processing for this component.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_enabled = true;
    };
}
