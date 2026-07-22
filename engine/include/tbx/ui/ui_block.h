#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gpu/shader_source.h"
#include "tbx/ui/document.h"
#include <utility>

namespace tbx::ui
{
    /// @brief
    /// Purpose: On-screen UI owned by a toy: an RML document shown while the toy lives and
    /// is enabled (the render graph's ui pass manages loading/visibility).
    struct TBX_API Ui : Block
    {
        assets::Handle<Document> document = {};
        assets::Handle<gpu::ShaderSource> vertex = {}; // custom stage; unset = the builtin ui.vert
        assets::Handle<gpu::ShaderSource> fragment =
            {}; // custom stage; unset = the builtin ui.frag

        // Anchors the document to the toy in the world: the ui pass projects the toy's
        // position and feeds the "anchor_<toy name>" slot a left/top style (or display:none
        // behind the camera) — label documents consume it via data-style.
        bool is_world_anchored = false;

        // Fluent setters — each returns *this for one-chain construction.
        Ui& set_document(assets::Handle<Document> value) { document = std::move(value); return *this; }
        Ui& set_vertex(assets::Handle<gpu::ShaderSource> value) { vertex = std::move(value); return *this; }
        Ui& set_fragment(assets::Handle<gpu::ShaderSource> value) { fragment = std::move(value); return *this; }
        Ui& set_world_anchored(bool value) { is_world_anchored = value; return *this; }
    };
}
