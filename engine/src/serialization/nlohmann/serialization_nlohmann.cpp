#include "tbx/serialization/json.h"

namespace tbx
{
    //// SERIALIZATION (nlohmann backend) ////

    std::string dump_json(const Json& data, const int indent)
    {
        return data.dump(indent);
    }

    bool is_valid_json(const std::string_view text)
    {
        return !Json::parse(text, nullptr, false).is_discarded();
    }

    Json parse_json(const std::string_view text)
    {
        auto parsed = Json::parse(text, nullptr, false);
        return parsed.is_discarded() ? Json {} : parsed;
    }
}
