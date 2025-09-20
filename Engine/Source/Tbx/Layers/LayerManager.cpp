#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerManager.h"

namespace Tbx
{
    void LayerManager::UpdateLayers()
    {
        for (auto& layer : _stack)
        {
            if (layer)
            {
                layer->Update();
            }
        }
    }

    Tbx::Ref<Layer> LayerManager::GetLayer(Tbx::uint index) const
    {
        if (index >= _stack.GetCount())
        {
            return nullptr;
        }

        return _stack[index];
    }

    Tbx::Ref<Layer> LayerManager::GetLayer(const std::string& name) const
    {
        for (auto& layer : _stack)
        {
            if (layer && layer->GetName() == name)
            {
                return layer;
            }
        }
        return nullptr;
    }

    std::vector<Tbx::Ref<Layer>> LayerManager::GetLayers() const
    {
        std::vector<Tbx::Ref<Layer>> layers = {};
        for (auto& layer : _stack)
        {
            if (layer)
            {
                layers.push_back(layer);
            }
        }

        return layers;
    }

    void LayerManager::AddLayer(const Tbx::Ref<Layer>& layer)
    {
        _stack.Push(layer);
    }

    void LayerManager::RemoveLayer(Tbx::uint index)
    {
        auto layer = GetLayer(index);
        if (layer)
        {
            _stack.Remove(layer);
        }
    }

    void LayerManager::RemoveLayer(const std::string& name)
    {
        auto layer = GetLayer(name);
        if (layer)
        {
            _stack.Remove(layer);
        }
    }

    void LayerManager::RemoveLayer(const Tbx::Ref<Layer>& layer)
    {
        if (layer)
        {
            _stack.Remove(layer);
        }
    }

    void LayerManager::ClearLayers()
    {
        _stack.Clear();
    }
}
