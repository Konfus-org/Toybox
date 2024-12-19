#pragma once

#include "LogLevel.h"
#include "ILogger.h"

#define TBX_TRACE(msg, ...)         std::cout << std::vformat(msg, std::make_format_args(__VA_ARGS__)) << std::endl
#define TBX_INFO(msg, ...)          std::cout << std::vformat(msg, std::make_format_args(__VA_ARGS__)) << std::endl
#define TBX_WARN(msg, ...)          std::cout << std::vformat(msg, std::make_format_args(__VA_ARGS__)) << std::endl
#define TBX_ERROR(msg, ...)         std::cout << std::vformat(msg, std::make_format_args(__VA_ARGS__)) << std::endl
#define TBX_CRITICAL(msg, ...)      std::cout << std::vformat(msg, std::make_format_args(__VA_ARGS__)) << std::endl
#define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_CRITICAL(msg, __VA_ARGS__); if(!(check)) __debugbreak()
