#pragma once
#include <nlohmann/json.hpp>

// The nlohmann serialization backend's TYPE surface — the parse/dump helpers are declared in
// tbx/serialization/serialization.h and implemented by this backend's serialization_nlohmann.cpp.
namespace tbx::serialization
{
    using Json = nlohmann::json;
}
