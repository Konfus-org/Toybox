#pragma once
#include "Tbx/Windowing/Window.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class IInputHandler;

    class TBX_EXPORT IWindowManager
    {
    public:
        virtual ~IWindowManager() = default;
        virtual void Update() const = 0;
        virtual Ref<Window> GetMainWindow() const = 0;
        virtual const std::vector<Ref<Window>>& GetAllWindows() const = 0;
        virtual Ref<Window> GetWindow(const Uid& id) const = 0;
        virtual Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080)) = 0;
        virtual void CloseWindow(const Uid& id) = 0;
        virtual void CloseAllWindows() = 0;
    };

    class TBX_EXPORT HeadlessWindow final : public Window
    {
    public:
        HeadlessWindow() = default;
        void Open() override {}
        void Close() override {}
        void Update() override {}
        void Focus() override {}
        bool IsClosed() override { return true; }
        bool IsFocused() override { return false; }
        NativeHandle GetNativeHandle() const override { return 0; }
        NativeWindow GetNativeWindow() const override { return nullptr; }
        const std::string& GetTitle() const override { static std::string title = "Headless"; return title; }
        void SetTitle(const std::string& title) override {}
        void SetMode(const WindowMode& mode) override {}
        WindowMode GetMode() const override { return WindowMode::Windowed; }
        const Size& GetSize() const override { static Size size = Size(0, 0); return size; }
        void SetSize(const Size& size) override {}
    };

    class TBX_EXPORT HeadlessWindowManager final : public IWindowManager
    {
    public:
        HeadlessWindowManager() = default;
        void Update() const override {}
        void CloseWindow(const Uid& id) override {}
        void CloseAllWindows() override {}
        Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080)) override
        {
            return _headlessWindow->Id;
        }
        Ref<Window> GetMainWindow() const override
        {
            return _headlessWindow;
        }
        const std::vector<Ref<Window>>& GetAllWindows() const override
        {
            static std::vector<Ref<Window>> windows = { _headlessWindow };
            return windows;
        }
        Ref<Window> GetWindow(const Uid& id) const override
        {
            return _headlessWindow;
        }

    private:
        Ref<HeadlessWindow> _headlessWindow = MakeRef<HeadlessWindow>();
    };

    class TBX_EXPORT WindowManager final : public IWindowManager
    {
    public:
        WindowManager() = default;
        WindowManager(
            Ref<IWindowFactory> windowFactory,
            Ref<IInputHandler> inputHandler,
            Ref<EventBus> eventBus);
        ~WindowManager() override;

        void Update() const override;

        Ref<Window> GetMainWindow() const override;
        const std::vector<Ref<Window>>& GetAllWindows() const override;
        Ref<Window> GetWindow(const Uid& id) const override;

        Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080)) override;

        void CloseWindow(const Uid& id) override;
        void CloseAllWindows() override;

    private:
        Ref<IWindowFactory> _windowFactory = {};
        Ref<IInputHandler> _inputHandler = nullptr;
        Ref<EventBus> _eventBus = {};
        Uid _mainWindowId = Uid::Invalid;
        WindowStack _stack = {};
    };
}
