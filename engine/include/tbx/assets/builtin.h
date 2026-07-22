#pragma once
#include "tbx/assets/handle.h"
#include "tbx/gpu/model.h"
#include "tbx/utils/uuid.h"

// Builtin assets: always available, no files involved. Reserved model handles the renderer
// resolves to its generated primitive meshes: gpu::Renderer {.model = builtin::CUBE}.
namespace tbx::builtin
{
    inline const assets::Handle<gpu::Model> CUBE =
        assets::Handle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 1});
    inline const assets::Handle<gpu::Model> PLANE =
        assets::Handle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 2});
    inline const assets::Handle<gpu::Model> SPHERE =
        assets::Handle<gpu::Model>(Uuid {.hi = 0xb001000000000000ull, .lo = 3});
}
