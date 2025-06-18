#pragma once
#include <nvrhi/nvrhi.h>

namespace NVRHIRendering
{
    class NVRHIMessageHandler : public nvrhi::IMessageCallback
    {
        void message(nvrhi::MessageSeverity severity, const char* messageText) final;
    };
}
