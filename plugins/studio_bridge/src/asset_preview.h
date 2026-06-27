#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Seeds a fresh asset-preview world with the shared base the editor's preview builds on: it
    /// force-registers the bundled sky material + its textures (so a C#-set sky material resolves) and adds
    /// a key directional light (so lit previews aren't black). The sky entity and the previewed asset's
    /// entity are created by the editor through the world/entity API — the bridge no longer builds them.
    void seed_preview_world(tbx::AssetManager& assets, tbx::World& world);

    /// @brief A built-in preview-mesh primitive exposed to the editor as a model asset (so a Renderer can
    /// show a material/texture on it): the registered in-memory model handle plus its editor-facing label.
    struct PreviewMeshAsset
    {
        tbx::Handle handle;
        std::string label;
    };

    /// @brief Registers the built-in preview-mesh primitives (sphere/cube/…) as in-memory model assets
    /// (idempotent) and returns them, so editor.listAssets can surface them and the editor can set a
    /// Renderer's model to one.
    std::vector<PreviewMeshAsset> register_preview_meshes(tbx::AssetManager& assets);
}
