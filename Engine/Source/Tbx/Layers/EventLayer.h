#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx
{
    class EventLayer : public Layer
    {
    public:
        explicit EventLayer(const std::weak_ptr<App>& app)
            : Layer("Events", app)
        {
        }

        void OnDetach() final;
    };
}
