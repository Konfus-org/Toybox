#pragma once
#include "Event.h"
#include "TbxAPI.h"
#include "KeyEvents.h"
#include "MouseEvents.h"
#include "WindowEvents.h"
#include "ApplicationEvents.h"
#include <typeindex>

namespace Tbx
{
    using EventCallback = std::function<bool(const Event&)>;

    class EventDispatcher
    {
    public:
        // Subscribe to a specific event type
        template <typename EventType>
        TBX_API static void Subscribe(const EventCallback& callback);

        // Unsubscribe from a specific event type
        template <typename EventType>
        TBX_API static void Unsubscribe(const EventCallback& callbackToRemove);

        // Send an event to all subscribers of its type
        template <typename EventType>
        TBX_API static void Send(const EventType& event);

    private:
        static std::unordered_map<std::type_index, std::vector<EventCallback>> _subscriptions;
    };

    // Explicit instantiations for event types
    template void TBX_API EventDispatcher::Subscribe<KeyPressedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyPressedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyPressedEvent>(const KeyPressedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyReleasedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyReleasedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyReleasedEvent>(const KeyReleasedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyHeldEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyHeldEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyHeldEvent>(const KeyHeldEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyRepeatedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyRepeatedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyRepeatedEvent>(const KeyRepeatedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseButtonPressedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseButtonPressedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseButtonPressedEvent>(const MouseButtonPressedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseButtonReleasedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseButtonReleasedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseButtonReleasedEvent>(const MouseButtonReleasedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseMovedEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseMovedEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseMovedEvent>(const MouseMovedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseScrolledEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseScrolledEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseScrolledEvent>(const MouseScrolledEvent& event);

    template void TBX_API EventDispatcher::Subscribe<WindowCloseEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<WindowCloseEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<WindowCloseEvent>(const WindowCloseEvent& event);

    template void TBX_API EventDispatcher::Subscribe<WindowResizeEvent>(const EventCallback& callback);
    template void TBX_API EventDispatcher::Unsubscribe<WindowResizeEvent>(const EventCallback& callbackToRemove);
    template void TBX_API EventDispatcher::Send<WindowResizeEvent>(const WindowResizeEvent& event);
}