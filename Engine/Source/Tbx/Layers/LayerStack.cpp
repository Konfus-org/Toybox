#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"
#include <algorithm>

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
            layer.reset();
        }
        _layers.clear();
    }

    void LayerStack::Push(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return;
        }

        layer->AttachTo(_layers);
    }

    void LayerStack::Remove(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return;
        }

        layer->DetachFrom(_layers);
    }

    void LayerStack::Remove(const std::string& name)
    {
        const auto it = std::find_if(
            _layers.begin(),
            _layers.end(),
            [&name](const Tbx::Ref<Layer>& layer)
            {
                return layer && layer->GetName() == name;
            });

        if (it != _layers.end())
        {
            Remove(*it);
        }
    }

    void LayerStack::Update()
    {
        for (auto& layer : _layers)
        {
            if (layer)
            {
                layer->Update();
            }
        }
    }

    Tbx::Ref<Layer> LayerStack::GetLayer(const std::string& name) const
    {
        const auto it = std::find_if(
            _layers.begin(),
            _layers.end(),
            [&name](const Tbx::Ref<Layer>& layer)
            {
                return layer && layer->GetName() == name;
            });

        if (it != _layers.end())
        {
            return *it;
        }

        return nullptr;
    }

    std::vector<Tbx::Ref<Layer>> LayerStack::GetLayers() const
    {
        std::vector<Tbx::Ref<Layer>> layers = {};
        layers.reserve(_layers.size());
        for (const auto& layer : _layers)
        {
            if (layer)
            {
                layers.push_back(layer);
            }
        }

        return layers;
    }
}
