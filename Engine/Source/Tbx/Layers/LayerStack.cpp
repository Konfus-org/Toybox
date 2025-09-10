#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    LayerStack::~LayerStack()
    {
        Clear();
    }

    void LayerStack::Clear()
    {
        for (auto& layer : std::ranges::reverse_view(_layers))
        {
            layer->OnDetach();
            layer.reset();
        }
        _layers.clear();
    }

    void LayerStack::Push(const std::shared_ptr<Layer>& layer)
    {
        _layers.emplace_back(layer);
        layer->OnAttach();
    }

    void LayerStack::Pop(const std::shared_ptr<Layer>& layer)
    {
        auto it = std::find(_layers.begin(), _layers.end(), layer);
        if (it != _layers.end())
        {
            (*it)->OnDetach();
            _layers.erase(it);
        }
    }
}

