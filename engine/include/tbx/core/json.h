#pragma once
#include <nlohmann/json.hpp>

namespace tbx
{
    // The one JSON seam: nothing outside this header names nlohmann. Swapping the JSON library
    // means re-pointing this alias (and the parse helpers that live on it).
    using Json = nlohmann::json;
}
