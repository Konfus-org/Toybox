#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gpu/material.h"
#include "tbx/gpu/model.h"
#include <utility>


namespace tbx::gpu
{
    /// @brief
    /// Purpose: Makes a toy visible: a model surfaced by a material. Builtin primitives are
    /// reserved model handles (tbx::builtin::CUBE/PLANE/SPHERE; unset renders the cube); an
    /// unset material renders the builtin white PBR surface.
    struct TBX_API Renderer : Block
    {
        assets::Handle<Material> material = {};
        assets::Handle<Model> model = {};

        // Fluent setters — each returns *this for one-chain construction.
        Renderer& set_material(assets::Handle<Material> value) { material = std::move(value); return *this; }
        Renderer& set_model(assets::Handle<Model> value) { model = std::move(value); return *this; }
    };
}
