#pragma once

#include "tbxpch.h"
#include "Log.h"
#include "ILogger.h"
#include "LogLevel.h"

#define TBX_TRACE(msg, ...)         ::Toybox::Log::Trace(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_INFO(msg, ...)          ::Toybox::Log::Info(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_WARN(msg, ...)          ::Toybox::Log::Warn(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_ERROR(msg, ...)         ::Toybox::Log::Error(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_CRITICAL(msg, ...)      ::Toybox::Log::Critical(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
