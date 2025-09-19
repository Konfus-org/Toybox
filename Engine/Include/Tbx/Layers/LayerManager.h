#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/TypeAliases/Int.h"
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

        EXPORT void AddLayer(const std::shared_ptr<Layer>& layer);
        EXPORT void RemoveLayer(Tbx::uint index);
        EXPORT void RemoveLayer(const std::string& name);
        EXPORT void RemoveLayer(const std::shared_ptr<Layer>& layer);

        EXPORT std::shared_ptr<Layer> GetLayer(Tbx::uint index) const;
        EXPORT std::shared_ptr<Layer> GetLayer(const std::string& name) const;

        template <typename T>
        EXPORT std::shared_ptr<T> GetLayer() const
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
        EXPORT std::vector<std::shared_ptr<T>> GetLayers() const
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

