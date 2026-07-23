#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: UI document asset (.html) — plain RML/RCSS text (Format::TEXT) handed to
    /// tbx::ui.
    struct TBX_SERIALIZABLE(SerializerFormat::TEXT, name="UiDocument") TBX_DLL_EXPORT Document : Asset
    {
        std::string text = {};
    };
}
