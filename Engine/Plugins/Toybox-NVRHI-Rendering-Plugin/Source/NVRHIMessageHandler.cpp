#include "NVRHIMessageHandler.h"
#include <Tbx/Systems/Debug/Debugging.h>

namespace NVRHIRendering
{
    void NVRHIMessageHandler::message(nvrhi::MessageSeverity severity, const char* messageText)
    {
        switch (severity)
        {
            case nvrhi::MessageSeverity::Info:
                TBX_INFO("NVRHI MSG: {}", messageText);
                break;
            case nvrhi::MessageSeverity::Warning:
                TBX_WARN("NVRHI MSG: {}", messageText);
                break;
            case nvrhi::MessageSeverity::Error:
                TBX_ERROR("NVRHI MSG: {}", messageText);
                break;
            case nvrhi::MessageSeverity::Fatal:
                TBX_CRITICAL("NVRHI MSG: {}", messageText);
                break;
            default:
                TBX_INFO("NVRHI MSG: {}", messageText);
                break;
        }
    }
}
