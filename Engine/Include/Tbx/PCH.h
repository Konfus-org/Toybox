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

#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"

#ifdef TBX_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <dlfcn.h>
#endif
