#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

// The nlohmann serialization backend: supplies the tbx::Json document type and the parse/dump
// helpers. Another backend folder (engine/src/serialization/<name>/) provides the same names
// to swap the library out (-DTBX_SERIALIZATION_BACKEND=<name>).
namespace tbx
{
    using Json = nlohmann::json;

    /// @brief
    /// Purpose: Serializes a document to text (indent < 0 = compact).
    inline std::string dump_json(const Json& data, const int indent = -1)
    {
        return data.dump(indent);
    }

    /// @brief
    /// Purpose: Parses text into a document; empty on malformed input (callers wrap with their
    /// own error context).
    inline Json parse_json(const std::string_view text)
    {
        auto parsed = Json::parse(text, nullptr, false);
        return parsed.is_discarded() ? Json {} : parsed;
    }

    /// @brief
    /// Purpose: True when text is well-formed JSON.
    inline bool is_valid_json(const std::string_view text)
    {
        return !Json::parse(text, nullptr, false).is_discarded();
    }
}
