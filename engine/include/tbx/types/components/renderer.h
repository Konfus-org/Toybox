#pragma once
#include "tbx/types/color.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/renderer.generated.h"
#include "tbx/types/handle.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Renders a Model asset for an entity, with optional per-slot material overrides.
    /// @details
    /// The only renderable component. `model` references the geometry (a Model asset; Static models
    /// share geometry, Dynamic models may mutate at runtime). `materials` overrides the model's
    /// per-slot default MaterialInstance handles: entry `i` overrides slot `i`, an empty handle
    /// inherits the model's own slot, and the vector may be shorter than the model's slot count.
    /// Ownership: Stores non-owning handle references. Thread Safety: Safe to copy between threads;
    /// synchronize mutation externally.
    [[serializable]];
    [[icon("Cuboid", Color::BLUE)]];
    struct TBX_API Renderer : Component
    {
        Renderer() = default;
        explicit Renderer(Handle model_handle);

        /// @brief Model asset handle that provides mesh geometry and default material slots.
        [[asset("fbx", "obj", "gltf", "glb", "model")]]
        Handle model = {};

        /// @brief Per-slot material assignments, aligned 1:1 with the model's hard material slots.
        /// Each entry is a MaterialInstance (.mti) or Material (.mat); the editor sizes this to the
        /// model's slot count and offers a picker per slot.
        [[asset("mti", "mat")]]
        std::vector<Handle> materials = {};
    };
}
