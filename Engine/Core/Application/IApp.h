#pragma once
#include "tbxpch.h"
#include "Windowing/IWindow.h"
#include "Layers/Layer.h"

namespace Toybox
{
    class IApp
    {
    public:
        virtual ~IApp() = default;

        virtual void Launch() = 0;
        virtual void Update() = 0;
        virtual void Close() = 0;
        virtual void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) = 0;
        virtual void PushLayer(const std::shared_ptr<Layer>& layer) = 0;
        virtual void PushOverlay(const std::shared_ptr<Layer>& layer) = 0;
        virtual bool IsRunning() const = 0;
        virtual std::string GetName() const = 0;
        virtual std::weak_ptr<IWindow> GetMainWindow() const = 0;
    };
}