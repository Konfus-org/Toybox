#pragma once
#include <Tbx/Plugin API/RegisterPlugin.h>

namespace SDLInput
{
    class SDLInputHandler : public Tbx::IInputHandlerPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

        void SetContext(const std::shared_ptr<Tbx::IWindow>& windowToListenTo) override;
        bool IsGamepadButtonDown(const int gamepadId, const int button) const override;
        bool IsGamepadButtonUp(const int gamepadId, const int button) const override;
        bool IsGamepadButtonHeld(const int gamepadId, const int button) const override;
        bool IsKeyDown(const int keyCode) const override;
        bool IsKeyUp(const int keyCode) const override;
        bool IsKeyHeld(const int keyCode) const override;
        bool IsMouseButtonDown(const int button) const override;
        bool IsMouseButtonUp(const int button) const override;
        bool IsMouseButtonHeld(const int button) const override;
        Tbx::Vector2 GetMousePosition() const override;
    };

    TBX_REGISTER_PLUGIN(SDLInputHandler);
}
