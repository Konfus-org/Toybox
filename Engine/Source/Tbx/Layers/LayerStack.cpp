#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"

namespace Tbx
{
    LayerStack::~LayerStack()
    {
        Clear();
    }

    bool LayerStack::Contains(const Uid& layerId) const
    {
        const auto& it = std::find_if(_layers.begin(), _layers.end(), [&layerId](const ExclusiveRef<Layer>&l) { return l->Id == layerId; });
        return it != _layers.end();
    }

    Layer& LayerStack::Get(const Uid& layerId)
    {
        const auto& layer = std::find_if(_layers.begin(), _layers.end(), [&layerId](const ExclusiveRef<Layer>& l) { return l->Id == layerId; });
        return **layer;
    }

    const std::vector<std::unique_ptr<Layer>>& LayerStack::GetAll() const
    {
        return _layers;
    }

    void LayerStack::Clear()
    {
        for (const auto& layer : _layers)
        {
            layer->OnDetach();
        }
        _layers.clear();
    }

    void LayerStack::Remove(const Uid& layerId)
    {
        auto it = std::find_if(_layers.begin(), _layers.end(), [&layerId](const ExclusiveRef<Layer>& l) { return l->Id == layerId; });
        if (it != _layers.end())
        {
            (*it)->OnDetach();
            _layers.erase(it);
        }
    }
}
