#pragma once
#include <Toybox.h>

namespace GlfwWindowing
{
    class GlfwWindow : public Toybox::Windowing::IWindow
    {
    public:
        GlfwWindow();
        ~GlfwWindow();

        void Open(Toybox::Windowing::WindowMode mode) override;
        void Update() override;

        void SetVSyncEnabled(bool enabled) override;
        bool const GetVSyncEnabled() const override;

        void SetSize(Toybox::Math::Size* size) override;
        const Toybox::Math::Size* GetSize() const override;

        const std::string GetTitle() const override;
        void SetTitle(const std::string& title) override;

        const Toybox::Math::uint64 GetId() const override;
        std::any GetNativeWindow() const override;

        void SetEventCallback(const EventCallbackFn& callback) override;
        void SetMode(Toybox::Windowing::WindowMode mode) override;

    private:
        std::string _title;
        Toybox::Math::Size* _size;
        bool _vSyncEnabled;
        EventCallbackFn _eventCallback;

        void SetupCallbacks();
        void SetupContext();
        void InitGlad();
        void InitGlfwIfNotAlreadyInitialized();
    };

    extern Toybox::Windowing::IWindow* Create()
    {
        return new GlfwWindow();
    }
}
