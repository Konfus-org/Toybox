#pragma once

#include "tbxpch.h"
#include "Log.h"
#include "ILogger.h"
#include "LogLevel.h"

#define TBX_TRACE(msg, ...)         ::Toybox::Debug::Log::Trace(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_INFO(msg, ...)          ::Toybox::Debug::Log::Info(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_WARN(msg, ...)          ::Toybox::Debug::Log::Warn(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_ERROR(msg, ...)         ::Toybox::Debug::Log::Error(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
#define TBX_CRITICAL(msg, ...)      ::Toybox::Debug::Log::Critical(std::vformat(msg, std::make_format_args(__VA_ARGS__)))
