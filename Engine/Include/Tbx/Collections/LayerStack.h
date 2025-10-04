#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Collections/Queryable.h"

namespace Tbx
{
    /// <summary>
    /// An application layer. Used to cleanly add and seperate functionality.
    /// Some examples are a graphics layer, windowing layer, input layer, etc...
    /// </summary>
    class TBX_EXPORT Layer
    {
    public:
        explicit(false) Layer(const std::string& name)
            : Name(name) {
        }
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        void Update();
        void FixedUpdate();
        void LateUpdate();

    protected:
        virtual void OnUpdate() {}
        virtual void OnFixedUpdate() {}
        virtual void OnLateUpdate() {}

    public:
        std::string Name = "";
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// Container that stores layers in registration order and assists with lifecycle notifications.
    /// </summary>
    class TBX_EXPORT LayerStack : public Queryable<Ref<Layer>>
    {
    public:
        ~LayerStack();

        template <typename TLayer, typename... TArgs>
        requires std::is_base_of_v<Layer, TLayer>
        Uid Push(TArgs&&... args)
        {
            auto layer = MakeExclusive<TLayer>(std::forward<TArgs>(args)...);
            const auto& layerId = layer->Id;
            layer->OnAttach();
            this->MutableItems().push_back(std::move(layer));
            return layerId;
        }

        void Remove(const Uid& layerId);
    };
}

