#pragma once
#include "gpu_resource_cache.h"
#include "render_validation.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/material_instance.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Packs a resolved material into the generic GPU material record shared by every
    /// surface and post-processing effect: the positional parameter float stream (declared .mat
    /// order) into params[], and each declared texture into a bindless slot through the cache.
    /// @details
    /// out_failure reports the first validation problem encountered (a missing texture, or parameter
    /// data overflowing the record); the record is still returned so the caller decides how to
    /// surface the failure. Render-lane only.
    GpuMaterialData pack_material(
        GpuResourceCache& cache,
        const Material& material,
        const std::string& material_name,
        RenderFailure& out_failure);

    /// @brief
    /// Purpose: Loads a material instance's base asset and applies its parameter/texture/config
    /// overrides into out_material, yielding the effective material to pack.
    /// @details
    /// Returns false with out_failure = MISSING_MATERIAL (and a warn-once) if the base asset cannot
    /// be loaded; out_name is a stable label for diagnostics. Render-lane only.
    bool resolve_material_instance(
        AssetManager& assets,
        const MaterialInstance& instance,
        Material& out_material,
        std::string& out_name,
        RenderFailure& out_failure);
}
