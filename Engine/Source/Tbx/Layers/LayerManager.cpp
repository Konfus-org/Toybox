#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerManager.h"

namespace Tbx
{
    void LayerManager::UpdateLayers()
    {
        for (auto& layer : _stack)
        {
            layer->Update();
        }
    }

    Tbx::Ref<Layer> LayerManager::GetLayer(Tbx::uint index) const
    {
        return _stack[index];
    }

    Tbx::Ref<Layer> LayerManager::GetLayer(const std::string& name) const
    {
        for (auto& layer : _stack)
        {
            if (layer->GetName() == name)
                return layer;
        }
        return nullptr;
    }

    void LayerManager::AddLayer(const Tbx::Ref<Layer>& layer)
    {
        _stack.Push(layer);
    }

    void LayerManager::RemoveLayer(Tbx::uint index)
    {
        _stack.Remove(GetLayer(index));
    }

    void LayerManager::RemoveLayer(const std::string& name)
    {
        _stack.Remove(GetLayer(name));
    }

    void LayerManager::RemoveLayer(const Tbx::Ref<Layer>& layer)
    {
        _stack.Remove(layer);
    }

    void LayerManager::ClearLayers()
    {
        _stack.Clear();
    }
}