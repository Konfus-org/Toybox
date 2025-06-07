#pragma once
#include "Tbx/Runtime/Layers/Layer.h"
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Input/InputCodes.h"
#include <Tbx/Math/Vectors.h>

namespace Tbx
{
    class Input : public Layer
    {
    public:
        Input(const std::string_view& name) : Layer(name) {}
        ~Input() override = default;

        bool IsOverlay() override;
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        EXPORT static bool IsGamepadButtonDown(const int gamepadId, const int button);
        EXPORT static bool IsGamepadButtonUp(const int gamepadId, const int button);
        EXPORT static bool IsGamepadButtonHeld(const int gamepadId, const int button);

        EXPORT static bool IsKeyDown(const int keyCode);
        EXPORT static bool IsKeyUp(const int keyCode);
        EXPORT static bool IsKeyHeld(const int keyCode);

        EXPORT static bool IsMouseButtonDown(const int button);
        EXPORT static bool IsMouseButtonUp(const int button);
        EXPORT static bool IsMouseButtonHeld(const int button);
        EXPORT static Vector2 GetMousePosition();

    private:
        static void OnWindowFocusChanged(const WindowFocusChangedEvent& args);

        static UID _windowFocusChangedEventId;
    };
}
