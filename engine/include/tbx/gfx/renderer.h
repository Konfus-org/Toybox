#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/material.h"
#include "tbx/reflection/attributes.h"
#include "tbx/gfx/model.h"
#include <utility>


namespace tbx
{
    /// @brief
    /// Purpose: Makes a toy visible: a model surfaced by a material. Builtin primitives are
    /// reserved model handles (tbx::Builtin::CUBE/PLANE/SPHERE; unset renders the cube); an
    /// unset material renders the builtin white PBR surface.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT Renderer : Block
    {
        AssetHandle<Material> material = {};
        AssetHandle<Model> model = {};

        // Fluent setters — each returns *this for one-chain construction.
        Renderer& set_material(AssetHandle<Material> value) { material = std::move(value); return *this; }
        Renderer& set_model(AssetHandle<Model> value) { model = std::move(value); return *this; }
    };
}
