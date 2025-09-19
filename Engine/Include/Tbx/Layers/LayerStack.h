#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/TypeAliases/Int.h"
#include <memory>
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
        /// Removes a layer from the stack.
        /// </summary>
        EXPORT void Remove(const std::shared_ptr<Layer>& layer);

        /// <summary>
        /// Returns the number of layers currently registered in the stack.
        /// </summary>
        EXPORT Tbx::uint GetCount() const { return static_cast<Tbx::uint>(_layers.size()); }

        EXPORT std::vector<std::shared_ptr<Layer>>::iterator begin() { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::iterator end() { return _layers.end(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return _layers.end(); }

    private:
        std::vector<std::shared_ptr<Layer>> _layers;
    };
}

