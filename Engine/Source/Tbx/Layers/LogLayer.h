#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Bridges engine logging facilities into the layer system so loggers live alongside other services.
    /// </summary>
    class LogLayer : public Layer
    {
    public:
        LogLayer(Tbx::Ref<ILoggerFactory> loggerFactory);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        Tbx::Ref<ILogger> _logger;
    };
}

