#pragma once
#include <nlohmann/json.hpp>

#define TBX_SERIALIZABLE_INTRUSIVE(Type, ...) NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, __VA_ARGS__)
#define TBX_SERIALIZABLE_NON_INTRUSIVE(Type, ...)                                                  \
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, __VA_ARGS__)
