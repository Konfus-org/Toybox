#pragma once
#include "Event.h"
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "KeyEvents.h"
#include "MouseEvents.h"
#include "WindowEvents.h"
#include "ApplicationEvents.h"
#include "Debug/DebugAPI.h"
#include <typeindex>
#include <mutex>

#define TBX_BIND_EVENT_CALLBACK(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    class TBX_API EventDispatcher
    {
    public:
        template <typename EventType>
        static void Subscribe(const std::function<void(EventType&)>& callback);

        template <typename EventType>
        static void Unsubscribe(const std::function<void(EventType&)>& callback);

        template <typename EventType>
        static void Send(EventType& event);

    private:
        static std::unordered_map<uint64, std::vector<std::function<void(Event&)>>> _subscribers;
        static std::mutex _mutex;
    };

    // Template definitions
    template void TBX_API EventDispatcher::Subscribe<KeyPressedEvent>(const std::function<void(KeyPressedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyPressedEvent>(const std::function<void(KeyPressedEvent&)>& callback);
    template void TBX_API EventDispatcher::Send<KeyPressedEvent>(KeyPressedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyReleasedEvent>(const std::function<void(KeyReleasedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyReleasedEvent>(const std::function<void(KeyReleasedEvent&)>& callback);
    template void TBX_API EventDispatcher::Send<KeyReleasedEvent>(KeyReleasedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyHeldEvent>(const std::function<void(KeyHeldEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyHeldEvent>(const std::function<void(KeyHeldEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyHeldEvent>(KeyHeldEvent& event);

    template void TBX_API EventDispatcher::Subscribe<KeyRepeatedEvent>(const std::function<void(KeyRepeatedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<KeyRepeatedEvent>(const std::function<void(KeyRepeatedEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<KeyRepeatedEvent>(KeyRepeatedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseButtonPressedEvent>(const std::function<void(MouseButtonPressedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseButtonPressedEvent>(const std::function<void(MouseButtonPressedEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseButtonPressedEvent>(MouseButtonPressedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseButtonReleasedEvent>(const std::function<void(MouseButtonReleasedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseButtonReleasedEvent>(const std::function<void(MouseButtonReleasedEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseButtonReleasedEvent>(MouseButtonReleasedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseMovedEvent>(const std::function<void(MouseMovedEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseMovedEvent>(const std::function<void(MouseMovedEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseMovedEvent>(MouseMovedEvent& event);

    template void TBX_API EventDispatcher::Subscribe<MouseScrolledEvent>(const std::function<void(MouseScrolledEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<MouseScrolledEvent>(const std::function<void(MouseScrolledEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<MouseScrolledEvent>(MouseScrolledEvent& event);

    template void TBX_API EventDispatcher::Subscribe<WindowCloseEvent>(const std::function<void(WindowCloseEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<WindowCloseEvent>(const std::function<void(WindowCloseEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<WindowCloseEvent>(WindowCloseEvent& event);

    template void TBX_API EventDispatcher::Subscribe<WindowResizeEvent>(const std::function<void(WindowResizeEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<WindowResizeEvent>(const std::function<void(WindowResizeEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<WindowResizeEvent>(WindowResizeEvent& event);

    template void TBX_API EventDispatcher::Subscribe<AppUpdateEvent>(const std::function<void(AppUpdateEvent&)>& callback);
    template void TBX_API EventDispatcher::Unsubscribe<AppUpdateEvent>(const std::function<void(AppUpdateEvent&)>& callbackToRemove);
    template void TBX_API EventDispatcher::Send<AppUpdateEvent>(AppUpdateEvent& event);
}