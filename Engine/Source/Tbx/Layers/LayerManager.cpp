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

    bool LayerManager::AddLayer(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        const auto existing = GetLayer(layer->GetName());
        TBX_ASSERT(!existing, "Layer names must be unique. A layer named {} is already registered.", layer->GetName());
        if (existing)
        {
            return false;
        }

        _stack.Push(layer);
        return true;
    }

    bool LayerManager::RemoveLayer(Tbx::uint index)
    {
        auto layer = GetLayer(index);
        if (!layer)
        {
            return false;
        }

        _stack.Remove(layer);
        return true;
    }

    bool LayerManager::RemoveLayer(const std::string& name)
    {
        auto layer = GetLayer(name);
        if (!layer)
        {
            return false;
        }

        _stack.Remove(layer);
        return true;
    }

    bool LayerManager::RemoveLayer(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        _stack.Remove(layer);
        return true;
    }

    void LayerManager::ClearLayers()
    {
        _stack.Clear();
    }
}
