#include "Tbx/App/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Time/DeltaTime.h"
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include <Tbx/Core/Events/EventDispatcher.h>

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
        _isRunning = false;
    }

    App::~App()
    {
        if (_isRunning) 
        {
            ShutdownSystems();
        }
    }
    
    void App::Launch(bool headless)
    {
        _isRunning = true;
        _isHeadless = headless;

        if (!_isHeadless)
        {
            // Subscribe to window events
            _windowClosedEventId = EventDispatcher::Subscribe<WindowClosedEvent>(TBX_BIND_CALLBACK(OnWindowClosed));
            _windowResizeEventId = EventDispatcher::Subscribe<WindowResizedEvent>(TBX_BIND_CALLBACK(OnWindowResize));

            // Init rendering
            RenderPipeline::Initialize();

            // Init input
            Input::Initialize();

            // Open main window
            WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
            auto mainWindow = WindowManager::GetMainWindow();
        }

        OnStart();
    }

    void App::Update()
    {
        // Update delta time
        Time::DeltaTime::Update();

        // Call on update for app inheritors
        OnUpdate();

        // Send update event
        AppUpdatedEvent updateEvent;
        EventDispatcher::Send(updateEvent);

        // Then update layers
        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }
    }

    void App::Close()
    {
        OnShutdown();
        ShutdownSystems();
    }

    void App::ShutdownSystems()
    {
        _isRunning = false;

        // Unsub to window events and shutdown events
        EventDispatcher::Unsubscribe(_windowClosedEventId);
        EventDispatcher::Unsubscribe(_windowResizeEventId);

        // Call detach on all layers
        for (const auto& layer : _layerStack)
        {
            layer->OnDetach();
        }

        if (!_isHeadless)
        {
            // Close all windows, shutdown rendering and input if we are not headless.
            WindowManager::CloseAllWindows();
            RenderPipeline::Shutdown();
            Input::Stop();
        }

        // Finally close the log and shutdown events, this should be the last thing to happen before modules are unloaded
        Log::Close();
        EventDispatcher::Clear();
    }

    void App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const
    {
        WindowManager::OpenNewWindow(name, mode, size);
    }

    void App::PushLayer(const std::shared_ptr<Layer>& layer)
    {
        _layerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void App::PushOverlay(const std::shared_ptr<Layer>& layer)
    {
        _layerStack.PushOverlay(layer);
    }

    void App::OnWindowClosed(WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == WindowManager::GetMainWindow().lock()->GetId())
        {
            // Stop running and close all windows
            _isRunning = false;
            WindowManager::CloseAllWindows();
        }
        else
        {
            // Find the window that was closed and remove it from the list, which will trigger its destruction once nothing no longer references it...
            WindowManager::CloseWindow(e.GetWindowId());
        }

        // Don't allow other things to process window close events as the window is about to be destroyed
        e.Handled = true;
    }

    void App::OnWindowResize(const WindowResizedEvent& e)
    {
        std::weak_ptr<IWindow> windowThatWasResized = WindowManager::GetWindow(e.GetWindowId());

        // TODO: Do this in the render pipeline!!!!
        // Draw the window while its resizing so there are no artifacts during the resize
        const bool& wasVSyncEnabled = RenderPipeline::IsVSyncEnabled();
        RenderPipeline::SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker
        RenderPipeline::Process(windowThatWasResized);
        RenderPipeline::SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized.lock()->GetSize();
        const auto& name = windowThatWasResized.lock()->GetTitle();
        TBX_INFO("Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return WindowManager::GetMainWindow();
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    bool App::IsRunning() const
    {
        return _isRunning;
    }
}
