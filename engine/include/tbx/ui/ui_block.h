#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/ui/ui_document.h"
#include "tbx/utils/api.h"

namespace tbx::ui
{
    /// @brief
    /// Purpose: On-screen UI owned by a toy: an RML document shown while the toy lives and
    /// is enabled (the render graph's ui pass manages loading/visibility).
    struct TBX_API Ui : ecs::Block
    {
        assets::AssetHandle<UiDocument> document = {};
        assets::AssetHandle<gfx::ShaderSource> vertex = {}; // custom stage; unset = the builtin ui.vert
        assets::AssetHandle<gfx::ShaderSource> fragment = {}; // custom stage; unset = the builtin ui.frag

        // Anchors the document to the toy in the world: the ui pass projects the toy's
        // position and feeds the "anchor_<toy name>" slot a left/top style (or display:none
        // behind the camera) — label documents consume it via data-style.
        bool is_world_anchored = false;
    };
}
