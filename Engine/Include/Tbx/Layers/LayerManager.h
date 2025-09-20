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

        EXPORT bool AddLayer(const Tbx::Ref<Layer>& layer);
        EXPORT bool RemoveLayer(Tbx::uint index);
        EXPORT bool RemoveLayer(const std::string& name);
        EXPORT bool RemoveLayer(const Tbx::Ref<Layer>& layer);

        EXPORT Tbx::Ref<Layer> GetLayer(Tbx::uint index) const;
        EXPORT Tbx::Ref<Layer> GetLayer(const std::string& name) const;
        EXPORT std::vector<Tbx::Ref<Layer>> GetLayers() const;

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
        EXPORT std::vector<Tbx::Ref<T>> GetLayers() const
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

