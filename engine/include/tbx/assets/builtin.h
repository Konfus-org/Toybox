#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/model.h"
#include "tbx/utils/uuid.h"

// Builtin assets: always available, no files involved. Reserved model handles the renderer
// resolves to its generated primitive meshes: Renderer {.model = builtin::CUBE}.
namespace tbx::builtin
{
    inline const AssetHandle<Model> CUBE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 1});
    inline const AssetHandle<Model> PLANE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 2});
    inline const AssetHandle<Model> SPHERE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 3});
}
