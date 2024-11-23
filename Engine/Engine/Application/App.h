#pragma once

#include "tbxpch.h"
#include "IWindow.h"
#include "ToyboxAPI.h"
#include "Layers/Layers.h"
#include "Events/Events.h"

namespace Toybox::Application
{
    class TOYBOX_API App
    {
    public:
        App(const std::string& name);
        virtual ~App();
        void Launch();
        void Update();
        void Close();
        void PushLayer(Layers::Layer* layer);
        void PushOverlay(Layers::Layer* layer);
        const bool IsRunning() const;
        const std::string& GetName() const;
        IWindow* GetMainWindow() const;

    protected:
        virtual void OnOpen() = 0;
        virtual void OnUpdate() = 0;
        virtual void OnClose() = 0;

    private:
        bool OnWindowClose(Events::WindowCloseEvent& e);
        void OnEvent(Events::Event& e);
    };

    // API to create app, meant to be defined in CLIENT!
    extern App* CreateApp();
}
