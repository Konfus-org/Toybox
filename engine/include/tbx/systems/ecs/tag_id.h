#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: A stable per-process id for a tag string. Runtime entity tags are stored as TagIds
    /// rather than strings so matching is an int compare and there is no per-entity string storage.
    /// Interning is collision-free (each distinct name gets a distinct id) — not a hash.
    using TagId = uint32;

    /// @brief The id of no tag. Reserved so a default-constructed TagId never matches a real tag.
    constexpr TagId INVALID_TAG_ID = 0U;

    /// @brief
    /// Purpose: Interns a tag name to its stable id, creating it on first use. Hierarchical: a dotted
    /// name (e.g. "editor.selected") records its parent segment ("editor") so ancestor matching works
    /// on ids. The same name always returns the same id within the process.
    /// @details
    /// Thread Safety: Safe to call concurrently. The id table is append-only and never shrinks (the tag
    /// vocabulary is small and long-lived).
    TBX_API TagId intern_tag(std::string_view name);

    /// @brief
    /// Purpose: The name a TagId was interned from, or an empty string for INVALID_TAG_ID / unknown id.
    /// @details
    /// Ownership: Returns a reference to the interned string, stable for the process lifetime. Thread
    /// Safety: Safe to call concurrently.
    TBX_API const std::string& tag_name(TagId id);

    /// @brief
    /// Purpose: Whether `ancestor` equals `tag` or names a parent segment of it — the hierarchical
    /// gameplay-tag rule ("editor" is an ancestor of "editor.selected", but not of "editorial"), on
    /// interned ids. INVALID_TAG_ID is an ancestor of nothing.
    /// @details
    /// Thread Safety: Safe to call concurrently.
    TBX_API bool tag_is_ancestor(TagId ancestor, TagId tag);
}
