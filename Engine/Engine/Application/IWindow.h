#pragma once
#include "tbxpch.h"
#include "ToyboxAPI.h"
#include "WindowMode.h"
#include "Events/Event.h"
#include "Math/Size.h"
#include "Math/Int64.h"

namespace Toybox::Application
{
    class TOYBOX_API IWindow
    {
    public:
        using EventCallbackFn = std::function<void(Events::Event&)>;

        virtual void Update() = 0;

        virtual void SetVSyncEnabled(bool enabled) = 0;
        virtual const bool GetVSyncEnabled() const = 0;

        virtual const Math::Size* GetSize() const = 0;
        virtual void SetSize(Math::Size* size) = 0;

        virtual const std::string GetTitle() const = 0;
        virtual const Math::uint64 GetId() const = 0;
        virtual std::any GetNativeWindow() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void SetMode(WindowMode mode) = 0;
    };
}
