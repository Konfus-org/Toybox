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
    /// @brief
    /// Purpose: Loads (and validates) a JSON document from disk. Lives at tbx scope because
    /// it specializes the tbx::load primary template (assets/load.h).
    template <>
    TBX_API Result<serialization::Json> load<serialization::Json>(
        const std::filesystem::path& path);
}
