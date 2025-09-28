#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Bridges engine logging facilities into the layer system so loggers live alongside other services.
    /// </summary>
    class LogLayer final : public Layer
    {
    public:
        LogLayer(Ref<ILoggerFactory> loggerFactory);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        Ref<ILogger> _logger;
    };
}

