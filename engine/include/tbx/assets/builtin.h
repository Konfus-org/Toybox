#pragma once
#include "tbx/assets/handle.h"
#include "tbx/gpu/model.h"
#include "tbx/utils/uuid.h"

// Builtin assets: always available, no files involved. Reserved model handles the renderer
// resolves to its generated primitive meshes: Renderer {.model = Builtin::CUBE}.
namespace tbx
{
    /// @brief
    /// Purpose: The reserved builtin model handles, grouped on a type so they read
    /// Builtin::CUBE at the flat tbx level (no builtin namespace).
    struct Builtin
    {
        static const AssetHandle<Model> CUBE;
        static const AssetHandle<Model> PLANE;
        static const AssetHandle<Model> SPHERE;
    };

    inline const AssetHandle<Model> Builtin::CUBE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 1});
    inline const AssetHandle<Model> Builtin::PLANE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 2});
    inline const AssetHandle<Model> Builtin::SPHERE =
        AssetHandle<Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 3});
}
