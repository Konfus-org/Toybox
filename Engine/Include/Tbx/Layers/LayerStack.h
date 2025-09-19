#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
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

        EXPORT std::vector<Tbx::Ref<Layer>>::iterator begin() { return _layers.begin(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::iterator end() { return _layers.end(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::const_iterator begin() const { return _layers.begin(); }
        EXPORT std::vector<Tbx::Ref<Layer>>::const_iterator end() const { return _layers.end(); }

    private:
        std::vector<Tbx::Ref<Layer>> _layers;
    };
}

