#include "Log.h"
#include "Modules/Modules.h"

#define TBX_VALIDATE_LOGGER(error_msg) if (_logger == nullptr) { std::cout << "Logger null! Falling back to std::cout " + error_msg << std::endl; return; }

namespace Toybox
{
	std::shared_ptr<ILogger> Log::_logger = nullptr;

	void Log::Open()
	{
		auto loggerFactory = ModuleServer::GetFactoryModule<ILogger>();
        auto sharedLogger = loggerFactory->CreateShared();
		_logger = sharedLogger;
		_logger->Open("Toybox::Runtime", "Log\\Toybox.log");
	}

	void Log::Close()
	{
		_logger->Close();
		_logger = nullptr;
	}

	void Log::Trace(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Trace, msg);
	}

	void Log::Info(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Info, msg);
	}

	void Log::Warn(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Warn, msg);
	}

	void Log::Error(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Error, msg);
	}

	void Log::Critical(const std::string& msg)
	{
		TBX_VALIDATE_LOGGER(msg);

		_logger->Log(LogLevel::Critical, msg);
	}
}