#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Container that stores layers in registration order and assists with lifecycle notifications.
    /// </summary>
    class TBX_EXPORT LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();

        LayerStack(const LayerStack&) = delete;
        LayerStack& operator=(const LayerStack&) = delete;
        LayerStack(LayerStack&&) noexcept = default;
        LayerStack& operator=(LayerStack&&) noexcept = default;

        bool Contains(const Uid& layerId) const;
        Layer& Get(const Uid& layerId);
        const std::vector<ExclusiveRef<Layer>>& GetAll() const;
        void Remove(const Uid& layerId);
        void Clear();
        uint Count() const { return static_cast<uint>(_layers.size()); }

        template <typename TLayer, typename... TArgs>
        requires std::is_base_of_v<Layer, TLayer>
        Uid Push(TArgs&&... args)
        {
            auto layer = std::make_unique<TLayer>(std::forward<TArgs>(args)...);
            const auto& layerId = layer->Id;
            layer->OnAttach();
            _layers.push_back(std::move(layer));
            return layerId;
        }

        auto begin() { return _layers.begin(); }
        auto end() { return _layers.end(); }
        auto begin() const { return _layers.begin(); }
        auto end() const { return _layers.end(); }

        const Layer& operator[](int index) const { return *_layers[index]; }

    private:
        std::vector<ExclusiveRef<Layer>> _layers = {};
    };
}

