#include "tbxpch.h"
#include "Log.h"
#include "ILogger.h"
#include "Modules/Modules.h"

namespace Toybox::Debug
{
	ILogger* _logger = Modules::ModuleServer::GetInstance()->GetModule<Modules::LoggerModule>()->Create();

	static void FallbackLog(std::string msg)
	{
		std::cout << msg << std::endl;
	}

	void Log::Trace(std::string msg)
	{
		if (_logger == nullptr)
		{
			FallbackLog("Trace: " + msg);
			return;
		}
		_logger->Log(LogLevel::Trace, msg);
	}

	void Log::Info(std::string msg)
	{
		if (_logger == nullptr)
		{
			FallbackLog("Info: " + msg);
			return;
		}
		_logger->Log(LogLevel::Info, msg);
	}

	void Log::Warn(std::string msg)
	{
		if (_logger == nullptr)
		{
			FallbackLog("Warn: " + msg);
			return;
		}
		_logger->Log(LogLevel::Warn, msg);
	}

	void Log::Error(std::string msg)
	{
		if (_logger == nullptr)
		{
			FallbackLog("Error: " + msg);
			return;
		}
		_logger->Log(LogLevel::Error, msg);
	}

	void Log::Critical(std::string msg)
	{
		if (_logger == nullptr)
		{
			FallbackLog("Critical: " + msg);
			return;
		}
		_logger->Log(LogLevel::Critical, msg);
	}

	template<typename... Args>
	void Log::Trace(std::string msg, Args&&... args)
	{
		va_start(args, msg);
		if (_logger == nullptr)
		{
			FallbackLog("Trace: " + std::format(msg, args));
			va_end(args);
			return;
		}
		_logger->Log(LogLevel::Trace, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Info(std::string msg, Args&&... args)
	{
		va_start(args, msg);
		if (_logger == nullptr)
		{
			FallbackLog("Info: " + std::format(msg, args));
			va_end(args);
			return;
		}
		_logger->Log(LogLevel::Info, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Warn(std::string msg, Args&&... args)
	{
		va_start(args, msg);
		if (_logger == nullptr)
		{
			FallbackLog("Warn: " + std::format(msg, args));
			va_end(args);
			return;
		}
		_logger->Log(LogLevel::Warn, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Error(std::string msg, Args&&... args)
	{
		va_start(args, msg);
		if (_logger == nullptr)
		{
			FallbackLog("Error: " + std::format(msg, args));
			va_end(args);
			return;
		}
		_logger->Log(LogLevel::Error, std::format(msg, args));
		va_end(args);
	}

	template<typename... Args>
	void Log::Critical(std::string msg, Args&&... args)
	{
		va_start(args, msg);
		if (_logger == nullptr)
		{
			FallbackLog("Critical: " + std::format(msg, args));
			va_end(args);
			return;
		}
		_logger->Log(LogLevel::Critical, std::format(msg, args));
		va_end(args);
	}
}