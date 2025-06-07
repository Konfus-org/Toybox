#pragma once
#include "Tbx/Runtime/Layers/Layer.h"

namespace Tbx
{
    class EventCoordinatorLayer : public Layer
    {
    public:
        explicit EventCoordinatorLayer(const std::string_view& name) : Layer(name) {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
