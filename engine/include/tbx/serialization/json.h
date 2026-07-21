#pragma once
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <string>
#include <string_view>
#include <tbx_serialization_backend.h>

namespace tbx::serialization
{
    /// @brief
    /// Purpose: True when text is well-formed JSON.
    TBX_API bool is_valid(std::string_view text);

    /// @brief
    /// Purpose: Serializes a document to text (indent < 0 = compact).
    TBX_API std::string dump(const Json& data, int indent = -1);

    /// @brief
    /// Purpose: Parses text into a document; empty on malformed input (callers wrap with their
    /// own error context).
    TBX_API Json parse(std::string_view text);
}

namespace tbx
{
    // The seam is organized under tbx::serialization; the type and the engine-wide verbs
    // stay reachable at tbx scope — the same shape every api uses (tbx::parse/dump/is_valid).
    using Json = serialization::Json;
    using serialization::dump;
    using serialization::is_valid;
    using serialization::parse;

    /// @brief
    /// Purpose: Loads (and validates) a JSON document from disk. Lives at tbx scope: it
    /// specializes the tbx::load primary template (assets/load.h).
    template <>
    TBX_API Result<Json> load<Json>(const std::filesystem::path& path);
}
