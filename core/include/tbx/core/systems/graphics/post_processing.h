#pragma once
#include "tbx/core/systems/graphics/material.h"
#include "tbx/core/tbx_api.h"
#include <initializer_list>
#include <utility>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes one material-driven post-processing step in the effect stack.
    /// @details
    /// Ownership: Stores a non-owning material handle reference and value settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API PostProcessingEffect
    {
        ~PostProcessingEffect();

        /// @brief
        /// Purpose: Runtime material data used to shade this fullscreen post-processing pass.
        /// @details
        /// Ownership: Owns parameter/texture override sets and a base material handle.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        MaterialInstance material = {};

        /// @brief
        /// Purpose: Enables or disables this effect without removing it from the stack.
        /// @details
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        bool is_enabled = true;

        /// @brief
        /// Purpose: Controls blend weight between source scene color (0) and effect output (1).
        /// @details
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        float blend = 1.0f;
    };

    /// @brief
    /// Purpose: Configures the scene-wide post-processing material for the final screen pass.
    /// @details
    /// Ownership: Stores value settings and an ordered effect stack by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API PostProcessing
    {
        ~PostProcessing();

        /// @brief
        /// Purpose: Ordered list of effects to apply from first to last.
        /// @details
        /// Ownership: Owns the effect stack vector and effect settings.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        std::vector<PostProcessingEffect> effects = {};

        /// @brief
        /// Purpose: Enables or disables post-processing for this component.
        /// @details
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        bool is_enabled = true;
    };
}
