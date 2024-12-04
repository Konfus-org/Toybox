#include "tbxpch.h"
#include "Log.h"
#include "LogLevel.h"
#include "Modules/Modules.h"

#define TBX_VALIDATE_LOGGER(error_msg) if (_logger == nullptr) { std::cout << "Logger null! Falling back to std::cout " + error_msg << std::endl; return; }

namespace Toybox
{
	ILogger* Log::_logger = nullptr;

	void Log::Open()
	{
		_logger = ((LoggerModule*)ModuleServer::GetModule("Spd Logger"))->CreateLogger("Toybox::Core");
	}

	void Log::Close()
	{
		((LoggerModule*)ModuleServer::GetModule("Spd Logger"))->DestroyLogger(_logger);
		_logger = nullptr;
	}

	void Log::Trace(std::string msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Trace, msg);
	}

	void Log::Info(std::string msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Info, msg);
	}

	void Log::Warn(std::string msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Warn, msg);
	}

	void Log::Error(std::string msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Error, msg);
	}

	void Log::Critical(std::string msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Critical, msg);
	}

	template<typename... Args>
	void Log::Trace(std::string msg, Args&&... args)
	{
		TBX_VALIDATE_LOGGER(msg);

		va_start(args, msg);
		_logger->Log(LogLevel::Trace, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Info(std::string msg, Args&&... args)
	{
		TBX_VALIDATE_LOGGER(msg);

		va_start(args, msg);
		_logger->Log(LogLevel::Info, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Warn(std::string msg, Args&&... args)
	{
		TBX_VALIDATE_LOGGER(msg);

		va_start(args, msg);
		_logger->Log(LogLevel::Warn, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Error(std::string msg, Args&&... args)
	{
		TBX_VALIDATE_LOGGER(msg);

		va_start(args, msg);
		_logger->Log(LogLevel::Error, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Critical(std::string msg, Args&&... args)
	{
		TBX_VALIDATE_LOGGER(msg);

		va_start(args, msg);
		_logger->Log(LogLevel::Critical, std::format(msg, args));
		va_end(args);
	}
}