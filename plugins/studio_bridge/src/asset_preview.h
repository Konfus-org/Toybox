#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: How an asset-preview view should frame its content — the point the orbit camera looks
    /// at and a good starting distance so the asset opens fully in view.
    struct AssetPreviewFraming
    {
        tbx::Vec3 target = tbx::Vec3(0.0F);
        float distance = 3.0F;
    };

    /// @brief
    /// Purpose: Populates `world` with the preview of the registered asset with the given id, presented
    /// per asset type from editor-driven, engine-provided data (the builtin.* catalog). `option` is a
    /// built-in mesh token a texture/material is shown on (or, for a material, "skybox"/"skysphere" to
    /// show it as the environment). `material_id` is the built-in surface material a model is shown
    /// under (0 = keep the model's own materials). `skybox_id` is the built-in sky material used as the
    /// background (0 = no sky, PREVIEW_SKYBOX_DEFAULT = the bundled day sky). Computes the orbit framing.
    /// @details
    /// Returns false when the asset id is unknown or its type is not previewable in the 3D viewer
    /// (e.g. a script/shader, which the editor opens in its text editor instead).
    bool build_asset_preview(
        tbx::AssetManager& assets,
        uint32 asset_id,
        const std::string& option,
        uint32 material_id,
        uint32 skybox_id,
        tbx::World& world,
        AssetPreviewFraming& out_framing);
}
