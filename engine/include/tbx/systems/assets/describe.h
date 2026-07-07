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
    /// Serializes the body when the type has a body writer, otherwise its flat meta fields (e.g. a texture's
    /// import settings). Returns an empty string when the type is not a registered, default-constructible
    /// asset with a JSON writer. Intended for editor tooling describing a settings/asset schema without a
    /// live instance.
    TBX_API std::string describe_serializable_asset(std::string_view type_name);

    /// @brief
    /// Purpose: Serializes a LOADED asset instance as the editor's enriched, attribute-rich schema — the same
    /// per-field { attributes, value } shape describe_serializable_asset emits, but carrying the instance's
    /// actual values rather than defaults. Used by the asset inspector so a material/texture shows its real
    /// state with type tokens, enum choices, and resizable-list element templates intact.
    /// @details
    /// Like describe_serializable_asset, this enters the editor scopes IN THE ENGINE MODULE so the generated
    /// serialize the registration invokes — gated on a per-module attribute thread-local — actually emits the
    /// attributes; calling the body/meta writer from a plugin under scopes entered there silently falls back to
    /// the lean { type, value } form. `asset` must point to an instance of the named type. Serializes the
    /// body when the type has a body writer, otherwise its flat meta fields (e.g. a texture's import settings).
    /// Returns an empty string when the type is unknown or has no JSON writer.
    TBX_API std::string describe_serializable_asset_instance(
        const void* asset, std::string_view type_name);
}
