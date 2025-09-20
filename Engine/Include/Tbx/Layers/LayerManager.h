#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Owns the layer stack and provides helpers for querying and mutating registered layers.
    /// </summary>
    class LayerManager
    {
    public:
        EXPORT void UpdateLayers();
        EXPORT void ClearLayers();

        EXPORT void AddLayer(const Ref<Layer>& layer);
        EXPORT void RemoveLayer(uint index);
        EXPORT void RemoveLayer(const std::string& name);
        EXPORT void RemoveLayer(const Ref<Layer>& layer);

        EXPORT Ref<Layer> GetLayer(uint index) const;
        EXPORT Ref<Layer> GetLayer(const std::string& name) const;
        EXPORT std::vector<Ref<Layer>> GetLayers() const;

        template <typename T>
        EXPORT Ref<T> GetLayer() const
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
        EXPORT std::vector<Ref<T>> GetLayers() const
        {
            std::vector<std::shared_ptr<T>> layers = {};
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

