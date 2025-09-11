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
            layer.reset();
        }
        _layers.clear();
    }

    void LayerStack::Push(const std::shared_ptr<Layer>& layer)
    {
        layer->AttachTo(_layers);
    }

    void LayerStack::Pop(const std::shared_ptr<Layer>& layer)
    {
        layer->DetachFrom(_layers);
    }

    void HasLayers::UpdateLayers()
    {
        for (auto& layer : _stack)
        {
            layer->Update();
        }
    }

    std::shared_ptr<Layer> HasLayers::GetLayer(Tbx::uint index) const
    {
        return _stack[index];
    }

    std::shared_ptr<Layer> HasLayers::GetLayer(const std::string& name) const
    {
        for (auto& layer : _stack)
        {
            if (layer->GetName() == name)
                return layer;
        }
        return nullptr;
    }

    void HasLayers::AddLayer(const std::shared_ptr<Layer>& layer)
    {
        _stack.Push(layer);
    }

    void HasLayers::RemoveLayer(Tbx::uint index)
    {
        _stack.Pop(GetLayer(index));
    }

    void HasLayers::RemoveLayer(const std::string& name)
    {
        _stack.Pop(GetLayer(name));
    }

    void HasLayers::RemoveLayer(const std::shared_ptr<Layer>& layer)
    {
        _stack.Pop(layer);
    }

    void HasLayers::ClearLayers()
    {
        _stack.Clear();
    }
}