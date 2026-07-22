#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/gfx/model.h"
#include "tbx/utils/uuid.h"

// Builtin assets: always available, no files involved. Reserved model handles the renderer
// resolves to its generated primitive meshes: Renderer {.model = builtin::CUBE}.
namespace tbx::builtin
{
    inline const assets::AssetHandle<Model> CUBE =
        assets::AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 1});
    inline const assets::AssetHandle<Model> PLANE =
        assets::AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 2});
    inline const assets::AssetHandle<Model> SPHERE =
        assets::AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 3});
}
