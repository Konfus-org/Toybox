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
    /// Purpose: Populates `world` with the preview of the registered asset with the given id, choosing
    /// the presentation by asset type and the `option` token: a texture/material on a chosen built-in
    /// mesh (or, for a material, "skybox"/"skysphere" to show it as the environment), or a model under
    /// a chosen built-in material ("metal"/"matte"/"unlit"/"original"). An empty option uses the
    /// type's default. Computes the orbit framing for the view.
    /// @details
    /// Returns false when the asset id is unknown or its type is not previewable in the 3D viewer
    /// (e.g. a script/shader, which the editor opens in its text editor instead).
    bool build_asset_preview(
        tbx::AssetManager& assets,
        uint32 asset_id,
        const std::string& option,
        const std::string& skybox,
        tbx::World& world,
        AssetPreviewFraming& out_framing);
}
