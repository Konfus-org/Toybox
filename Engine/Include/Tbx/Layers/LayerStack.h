#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Int.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Container that stores layers in registration order and assists with lifecycle notifications.
    /// </summary>
    class TBX_EXPORT LayerStack
    {
    public:
        ~LayerStack();

        /// <summary>
        /// Operator to allow indexing into the layer stack.
        /// </summary>
        const Layer& operator[](int index) const { return _layers[index]; }

        /// <summary>
        /// Clear the layer stack.
        /// </summary>
        void Clear();

        /// <summary>
        /// Adds a layer onto the top of the stack.
        /// </summary>
        template <typename TLayer, typename... TArgs>
        Uid Push(TArgs&&... args)
        {
            static_assert(std::is_base_of<Layer, TLayer>::value, "TLayer must be a subclass of Layer");
            auto layer = TLayer(std::forward<TArgs>(args)...);
            layer.AttachTo(_layers);
            return layer.Id;
        }

        /// <summary>
        /// Removes a layer from the stack.
        /// </summary>
        void Remove(const Uid& layer);

        /// <summary>
        /// Returns the number of layers currently registered in the stack.
        /// </summary>
        uint GetSize() const { return static_cast<uint>(_layers.size()); }

        std::vector<Layer>::iterator begin() { return _layers.begin(); }
        std::vector<Layer>::iterator end() { return _layers.end(); }
        std::vector<Layer>::const_iterator begin() const { return _layers.begin(); }
        std::vector<Layer>::const_iterator end() const { return _layers.end(); }

    private:
        std::vector<Layer> _layers;
    };
}

