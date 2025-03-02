#include "TbxPCH.h"
#include "Log.h"
#include "LogLevel.h"
#include "FallbackLogger.h"
#include "Plugins/PluginServer.h"
#include <iostream>

namespace Tbx
{
	std::shared_ptr<ILogger> Log::_logger;

	void Log::Open(const std::string& name, const std::string& logSaveLocation)
	{
		_logger = std::make_shared<FallbackLogger>();
		_logger->Open(name, logSaveLocation);

		auto pluginLogger = PluginServer::GetPlugin<ILogger>();

		TBX_VALIDATE_PTR(pluginLogger, "Failed to load logger plugin!");

		_logger = pluginLogger;
		_logger->Open(name, logSaveLocation);
	}

	void Log::Close()
	{
		_logger->Close();
		_logger.reset();
	}

	void Log::Trace(const std::string& msg)
	{
		_logger->Log(static_cast<int>(LogLevel::Trace), msg);
	}

	void Log::Info(const std::string& msg)
	{
		_logger->Log(static_cast<int>(LogLevel::Info), msg);
	}

	void Log::Warn(const std::string& msg)
	{
		_logger->Log(static_cast<int>(LogLevel::Warn), msg);
	}

	void Log::Error(const std::string& msg)
	{
		_logger->Log(static_cast<int>(LogLevel::Error), msg);
	}

	void Log::Critical(const std::string& msg)
	{
		_logger->Log(static_cast<int>(LogLevel::Critical), msg);
	}
}