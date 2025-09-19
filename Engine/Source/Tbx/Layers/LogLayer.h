#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Debug/ILogger.h"

namespace Tbx
{
    /// <summary>
    /// Bridges engine logging facilities into the layer system so loggers live alongside other services.
    /// </summary>
    class LogLayer : public Layer
    {
    public:
        LogLayer(std::shared_ptr<ILoggerFactory> loggerFactory);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        std::shared_ptr<ILogger> _logger;
    };
}

