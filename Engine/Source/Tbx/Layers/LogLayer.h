#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class LogLayer : public Layer
    {
    public:
        LogLayer(const std::weak_ptr<App>& app)
            : Layer("Logging", app)
        {
        }

        void OnAttach() override;
        void OnDetach() override;
    };
}

