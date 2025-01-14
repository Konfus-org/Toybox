#pragma once
#include "Event.h"
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "Callback.h"
#include "KeyEvents.h"
#include "MouseEvents.h"
#include "WindowEvents.h"
#include "ApplicationEvents.h"
#include "Debug/DebugAPI.h"
#include <typeindex>
#include <mutex>

#define TBX_BIND_CALLBACK(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    class Events
    {
    public:
        template <class TEvent>
        TBX_API static UUID Subscribe(const CallbackFunction<TEvent>& callback);

        template <class TEvent>
        TBX_API static void Unsubscribe(const UUID& callbackToUnsub);

        template <class TEvent>
        TBX_API static void Send(TEvent& event);

    private:
        static std::unordered_map<hash, std::vector<Callback<Event>>> _subscribers;
        static std::mutex _mutex;
    };

    // Template definitions
    template UUID TBX_API Events::Subscribe<KeyPressedEvent>(const CallbackFunction<KeyPressedEvent>& callback);
    template void TBX_API Events::Unsubscribe<KeyPressedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<KeyPressedEvent>(KeyPressedEvent& event);

    template UUID TBX_API Events::Subscribe<KeyReleasedEvent>(const CallbackFunction<KeyReleasedEvent>& callback);
    template void TBX_API Events::Unsubscribe<KeyReleasedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<KeyReleasedEvent>(KeyReleasedEvent& event);

    template UUID TBX_API Events::Subscribe<KeyHeldEvent>(const CallbackFunction<KeyHeldEvent>& callback);
    template void TBX_API Events::Unsubscribe<KeyHeldEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<KeyHeldEvent>(KeyHeldEvent& event);

    template UUID TBX_API Events::Subscribe<KeyRepeatedEvent>(const CallbackFunction<KeyRepeatedEvent>& callback);
    template void TBX_API Events::Unsubscribe<KeyRepeatedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<KeyRepeatedEvent>(KeyRepeatedEvent& event);

    template UUID TBX_API Events::Subscribe<MouseButtonPressedEvent>(const CallbackFunction<MouseButtonPressedEvent>& callback);
    template void TBX_API Events::Unsubscribe<MouseButtonPressedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<MouseButtonPressedEvent>(MouseButtonPressedEvent& event);

    template UUID TBX_API Events::Subscribe<MouseButtonReleasedEvent>(const CallbackFunction<MouseButtonReleasedEvent>& callback);
    template void TBX_API Events::Unsubscribe<MouseButtonReleasedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<MouseButtonReleasedEvent>(MouseButtonReleasedEvent& event);

    template UUID TBX_API Events::Subscribe<MouseMovedEvent>(const CallbackFunction<MouseMovedEvent>& callback);
    template void TBX_API Events::Unsubscribe<MouseMovedEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<MouseMovedEvent>(MouseMovedEvent& event);

    template UUID TBX_API Events::Subscribe<MouseScrolledEvent>(const CallbackFunction<MouseScrolledEvent>& callback);
    template void TBX_API Events::Unsubscribe<MouseScrolledEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<MouseScrolledEvent>(MouseScrolledEvent& event);

    template UUID TBX_API Events::Subscribe<WindowCloseEvent>(const CallbackFunction<WindowCloseEvent>& callback);
    template void TBX_API Events::Unsubscribe<WindowCloseEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<WindowCloseEvent>(WindowCloseEvent& event);

    template UUID TBX_API Events::Subscribe<WindowResizeEvent>(const CallbackFunction<WindowResizeEvent>& callback);
    template void TBX_API Events::Unsubscribe<WindowResizeEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<WindowResizeEvent>(WindowResizeEvent& event);

    template UUID TBX_API Events::Subscribe<AppUpdateEvent>(const CallbackFunction<AppUpdateEvent>& callback);
    template void TBX_API Events::Unsubscribe<AppUpdateEvent>(const UUID& callbackToUnsub);
    template void TBX_API Events::Send<AppUpdateEvent>(AppUpdateEvent& event);
}