#pragma once
#include "tbxpch.h"
#include "WindowMode.h"
#include "Events/Event.h"
#include "Math/Size.h"
#include "Math/Int.h"

namespace Toybox
{
    class IWindow
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        IWindow() = default;
        virtual ~IWindow() = default;

        virtual void Open(WindowMode mode) = 0;
        virtual void Update() = 0;

        virtual void SetVSyncEnabled(bool enabled) = 0;
        virtual bool GetVSyncEnabled() const = 0;

        virtual Size GetSize() const = 0;
        virtual void SetSize(Size size) = 0;

        virtual std::string GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual uint64 GetId() const = 0;
        virtual std::any GetNativeWindow() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void SetMode(WindowMode mode) = 0;
    };
}
