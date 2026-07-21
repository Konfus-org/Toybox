#pragma once
// The one serialization seam: the selected backend (cmake tbx_backend(SERIALIZATION ...))
// supplies the tbx::Json type via its <tbx_serialization_backend.h> and implements the helpers
// declared here in its own .cpp — swapped at link time like every other backend. Nothing else
// names the library.
#include <tbx_serialization_backend.h>
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Serializes a document to text (indent < 0 = compact).
    std::string dump_json(const Json& data, int indent = -1);

    /// @brief
    /// Purpose: True when text is well-formed JSON.
    bool is_valid_json(std::string_view text);

    /// @brief
    /// Purpose: Parses text into a document; empty on malformed input (callers wrap with their
    /// own error context).
    Json parse_json(std::string_view text);
}
