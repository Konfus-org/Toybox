#pragma once
// The engine's precompiled header: the std headers every TU re-parses plus the header-only
// third-party seams. Build-speed only — source files still include what they use.
#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <tbx_ecs_backend.h>
#include <tbx_math_backend.h>
#include <tbx_serialization_backend.h>
