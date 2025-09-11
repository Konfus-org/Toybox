#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/TypeAliases/Int.h"
#include <memory>
#include <vector>

namespace Tbx
{
    class LayerStack
    {
    public:
        EXPORT ~LayerStack();

        /// <summary>
        /// Operator to allow indexing into the layer stack.
        /// </summary>
        EXPORT std::shared_ptr<Layer> operator[](int index) const { return _layers[index]; }

        /// <summary>
        /// Clear the layer stack.
        /// </summary>
        EXPORT void Clear();

        /// <summary>
        /// Push a layer onto the top of the stack.
        /// </summary>
        EXPORT void Push(const std::shared_ptr<Layer>& layer);

        /// <summary>
        /// Pop a layer from the top of the stack.
        /// </summary>
        EXPORT void Pop(const std::shared_ptr<Layer>& layer);

        EXPORT std::vector<std::shared_ptr<Layer>>::iterator begin() { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::iterator end() { return _layers.end(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return _layers.end(); }

    private:
        std::vector<std::shared_ptr<Layer>> _layers;
    };

    class HasLayers
    {
    public:
        EXPORT void UpdateLayers();

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

        EXPORT void AddLayer(const std::shared_ptr<Layer>& layer);

        EXPORT void RemoveLayer(Tbx::uint index);
        EXPORT void RemoveLayer(const std::string& name);
        EXPORT void RemoveLayer(const std::shared_ptr<Layer>& layer);

        EXPORT void ClearLayers();

    private:
        LayerStack _stack;
    };
}

