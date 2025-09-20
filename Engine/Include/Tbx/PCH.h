#pragma once

#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <any>
#include <array>
#include <queue>
#include <string>
#include <format>
#include <vector>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <ranges>
#include <filesystem>

#ifdef TBX_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif
