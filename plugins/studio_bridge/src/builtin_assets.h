#pragma once
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include <string>
#include <vector>

namespace tbx::studio_bridge::builtin
{
    /// @brief Resolves a built-in preview mesh token (e.g. "sphere"/"cube"/"quad") to its mesh;
    /// unknown or empty tokens fall back to `fallback`. The editor sends the token; only the bridge
    /// needs the mapping, so the mesh list itself stays private to the implementation.
    const tbx::Mesh& mesh_for(const std::string& token, const tbx::Mesh& fallback);

    /// @brief The handles of the engine/bridge-provided preview assets (the surface + sky materials and
    /// the sky textures) the editor surfaces through editor.listAssets. Built-in meshes are not here —
    /// they are engine primitives, not registered assets.
    const std::vector<tbx::Handle>& assets();
}
