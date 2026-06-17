#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Serializes a default-constructed instance of the named asset type as the editor's enriched,
    /// attribute-rich schema — every field plus its { attributes, value, is_default } metadata (type tokens,
    /// enum choices, and a resizable vector's element_template), the same per-field shape entity.describe
    /// emits for components.
    /// @details
    /// The attribute enrichment relies on AttributeSerializationScope, whose switch is a per-module
    /// thread-local: it only affects a serialize running in the same module that entered the scope. This
    /// helper lives in the engine module alongside the generated serialize, so it produces the enriched shape
    /// where calling serialize() from a plugin would silently fall back to the lean { type, value } form.
    /// Returns an empty string when the type is not a registered, default-constructible asset with a JSON body
    /// writer. Intended for editor tooling describing a settings/asset schema without a live instance.
    TBX_API std::string describe_serializable_asset(std::string_view type_name);
}
