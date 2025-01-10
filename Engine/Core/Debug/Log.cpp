#include "TbxPCH.h"
#include "Log.h"
#include "LogLevel.h"
#include "Plugins/PluginServer.h"

#define TBX_VALIDATE_LOGGER(error_msg) if (_logger == nullptr) { std::cout << "Logger null! Falling back to std::cout " + error_msg << std::endl; return; }

namespace Tbx
{
	std::shared_ptr<ILogger> Log::_logger;

	void Log::Open(const std::string& name, const std::string& logSaveLocation)
	{
		const auto& loggerFactory = PluginServer::GetPlugin<ILogger>();
		auto sharedLogger = loggerFactory.lock()->CreateShared();
		_logger = sharedLogger;
		_logger->Open(name, logSaveLocation);
	}

	void Log::Close()
	{
		_logger->Close();
		_logger.reset();
	}

	void Log::Trace(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);
		_logger->Log(static_cast<int>(LogLevel::Trace), msg);
	}

	void Log::Info(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);
		_logger->Log(static_cast<int>(LogLevel::Info), msg);
	}

	void Log::Warn(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);
		_logger->Log(static_cast<int>(LogLevel::Warn), msg);
	}

	void Log::Error(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);
		_logger->Log(static_cast<int>(LogLevel::Error), msg);
	}

	void Log::Critical(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);
		_logger->Log(static_cast<int>(LogLevel::Critical), msg);
	}
}