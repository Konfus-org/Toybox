#pragma once
#include <Tbx/Windowing/IWindow.h>
#include <SDL3/SDL.h>

namespace SDLWindowing
{
    class SDLWindow : public Tbx::IWindow
    {
    public:
        SDLWindow(const std::string& title, const Tbx::Size& size)
            : _title(title), _size(size) {}
        ~SDLWindow() override;

        Tbx::NativeHandle GetNativeHandle() const override;
        Tbx::NativeWindow GetNativeWindow() const override;
        void Open(const Tbx::WindowMode& mode) override;
        void Close() override;
        void Update() override;
        void Focus() override;
        const std::string& GetTitle() const override;
        void SetTitle(const std::string& title) override;
        const Tbx::Size& GetSize() const override;
        void SetSize(const Tbx::Size& size) override;
        void SetMode(const Tbx::WindowMode& mode) override;

    private:
        SDL_Window* _window = nullptr;
        Tbx::WindowMode _currentMode = Tbx::WindowMode::Windowed;
        Tbx::Size _size = { 800, 600 };
        std::string _title = "";
    };
}

