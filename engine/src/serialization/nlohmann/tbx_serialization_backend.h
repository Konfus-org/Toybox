#pragma once
#include <nlohmann/json.hpp>

// The nlohmann serialization backend's TYPE surface: the Json document alias.
namespace tbx::serialization
{
    using Json = nlohmann::json;
}
