#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/ui/ui_document.h"
#include "tbx/gfx/shader_source.h"

namespace tbx
{
    /// @brief
    /// Purpose: On-screen UI owned by a toy: an RML document shown while the toy lives and
    /// is enabled (the render graph's ui pass manages loading/visibility).
    struct TBX_API Ui
    {
        AssetHandle<UiDocument> document = {};
        AssetHandle<ShaderSource> vertex = {};   // custom stage; unset = the builtin ui.vert
        AssetHandle<ShaderSource> fragment = {}; // custom stage; unset = the builtin ui.frag
        bool is_visible = true;
    };
}
