#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Math/Int.h"
#include <memory>
#include <vector>
#include "Tbx/Memory/Refs/Refs.h"

namespace Tbx
{
    class LayerManager
    {
    public:
        EXPORT void UpdateLayers();
        EXPORT void ClearLayers();

        EXPORT void AddLayer(const Tbx::Ref<Layer>& layer);
        EXPORT void RemoveLayer(Tbx::uint index);
        EXPORT void RemoveLayer(const std::string& name);
        EXPORT void RemoveLayer(const Tbx::Ref<Layer>& layer);

        EXPORT Tbx::Ref<Layer> GetLayer(Tbx::uint index) const;
        EXPORT Tbx::Ref<Layer> GetLayer(const std::string& name) const;

        template <typename T>
        EXPORT Tbx::Ref<T> GetLayer() const
        {
            for (auto layer : _stack)
            {
                auto castedLayer = std::dynamic_pointer_cast<T>(layer);
                if (castedLayer)
                {
                    return castedLayer;
                }
            }
            return nullptr;
        }

        template <typename T>
        EXPORT Tbx::Ref<T> GetLayers() const
        {
            auto layers = std::vector<T>();
            for (auto layer : _stack)
            {
                auto castedLayer = std::dynamic_pointer_cast<T>(layer);
                if (castedLayer)
                {
                    layers.push_back(castedLayer);
                }
            }
            return layers;
        }

    private:
        LayerStack _stack;
    };
}

