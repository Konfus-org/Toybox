#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Container that stores layers in registration order and assists with lifecycle notifications.
    /// </summary>
    class LayerStack
    {
    public:
        EXPORT ~LayerStack();

        /// <summary>
        /// Operator to allow indexing into the layer stack.
        /// </summary>
        EXPORT Tbx::Ref<Layer> operator[](int index) const { return _layers[index]; }

        /// <summary>
        /// Clear the layer stack.
        /// </summary>
        EXPORT void Clear();

        /// <summary>
        /// Push a layer onto the top of the stack.
        /// </summary>
        EXPORT void Push(const Tbx::Ref<Layer>& layer);

        /// <summary>
        /// Removes a layer from the stack.
        /// </summary>
        EXPORT void Remove(const Tbx::Ref<Layer>& layer);
        EXPORT void Remove(const std::string& name);

        /// <summary>
        /// Returns the number of layers currently registered in the stack.
        /// </summary>
        EXPORT Tbx::uint GetCount() const { return static_cast<Tbx::uint>(_layers.size()); }

        EXPORT std::vector<Tbx::Ref<Layer>>::iterator begin() { return _layers.begin(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::iterator end() { return _layers.end(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::const_iterator begin() const { return _layers.begin(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::const_iterator end() const { return _layers.end(); }

        /// <summary>
        /// Executes the update loop for all registered layers.
        /// </summary>
        EXPORT void Update();

        /// <summary>
        /// Retrieves a layer by name.
        /// </summary>
        EXPORT Tbx::Ref<Layer> GetLayer(const std::string& name) const;

        /// <summary>
        /// Returns all registered layers.
        /// </summary>
        EXPORT std::vector<Tbx::Ref<Layer>> GetLayers() const;

        /// <summary>
        /// Retrieves the first layer of the requested type.
        /// </summary>
        template <typename TLayer>
        Tbx::Ref<TLayer> GetLayer() const
        {
            static_assert(std::is_base_of_v<Layer, TLayer>, "TLayer must derive from Layer");
            for (const auto& layer : _layers)
            {
                auto typedLayer = std::dynamic_pointer_cast<TLayer>(layer);
                if (typedLayer)
                {
                    return typedLayer;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// Retrieves all layers of the requested type.
        /// </summary>
        template <typename TLayer>
        std::vector<Tbx::Ref<TLayer>> GetLayersOfType() const
        {
            static_assert(std::is_base_of_v<Layer, TLayer>, "TLayer must derive from Layer");
            std::vector<Tbx::Ref<TLayer>> layers = {};
            for (const auto& layer : _layers)
            {
                auto typedLayer = std::dynamic_pointer_cast<TLayer>(layer);
                if (typedLayer)
                {
                    layers.push_back(typedLayer);
                }
            }
            return layers;
        }

    private:
        std::vector<Tbx::Ref<Layer>> _layers;
    };
}

