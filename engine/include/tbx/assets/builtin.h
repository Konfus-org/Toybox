#pragma once
#include "tbx/assets/handle.h"
#include "tbx/gpu/model.h"
#include "tbx/utils/uuid.h"

// Builtin assets: always available, no files involved. Reserved model handles the renderer
// resolves to its generated primitive meshes: gpu::Renderer {.model = Builtin::CUBE}.
namespace tbx
{
    /// @brief
    /// Purpose: The reserved builtin model handles, grouped on a type so they read
    /// Builtin::CUBE at the flat tbx level (no builtin namespace).
    struct Builtin
    {
        static const AssetHandle<gpu::Model> CUBE;
        static const AssetHandle<gpu::Model> PLANE;
        static const AssetHandle<gpu::Model> SPHERE;
    };

    inline const AssetHandle<gpu::Model> Builtin::CUBE =
        AssetHandle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 1});
    inline const AssetHandle<gpu::Model> Builtin::PLANE =
        AssetHandle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 2});
    inline const AssetHandle<gpu::Model> Builtin::SPHERE =
        AssetHandle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 3});
}
