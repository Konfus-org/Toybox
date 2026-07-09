#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Which intermediate of the forward pipeline the material shaders output in place of
    /// the shaded color — the editor's "render stage" debug views. FINAL is the normal lit frame.
    /// @details
    /// Ownership: Value enum copied by value. Thread Safety: Immutable; safe for concurrent reads.
    /// Values mirror the TBX_DEBUG_STAGE_* constants in ShaderBase.glsl — keep them in sync.
    enum class RenderDebugStage : uint32
    {
        FINAL = 0,
        DIFFUSE = 1,
        NORMALS = 2,
        SHADOWS = 3,
        DEPTH = 4,
    };

    /// @brief
    /// Purpose: A debug view over the rendering pipeline: force one render stage as the output
    /// and/or disable post-processing, applied only to cameras matching the tag gate (any shared
    /// tag matches, the same semantic render passes use; an empty gate applies to every camera).
    /// Tooling state (e.g. an editor's render-layers toolbar), never serialized.
    /// @details
    /// Ownership: Owns its tag strings by value. Thread Safety: Copied under the pipeline's lock;
    /// safe to build on any thread and hand to Rendering::set_debug_view.
    struct TBX_API RenderDebugView
    {
        RenderDebugStage stage = RenderDebugStage::FINAL;
        bool post_processing_enabled = true;
        std::vector<std::string> camera_tags = {};
    };
}
