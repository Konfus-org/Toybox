#pragma once
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Plugins/Plugin.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <array>
#include <unordered_map>

namespace Tbx::Plugins::SDLInput
{
	class SDLInputHandlerPlugin final
		: public Plugin
		, public IInputHandler
	{
	public:
		SDLInputHandlerPlugin(Ref<EventBus> eventBus);
		~SDLInputHandlerPlugin() override;

		void Update() override;

		bool IsGamepadButtonDown(int playerIndex, int button) const override;
		bool IsGamepadButtonUp(int playerIndex, int button) const override;
		bool IsGamepadButtonHeld(int playerIndex, int button) const override;
		float GetGamepadAxis(int playerIndex, int axis) const override;

		bool IsKeyDown(int keyCode) const override;
		bool IsKeyUp(int keyCode) const override;
		bool IsKeyHeld(int keyCode) const override;

		bool IsMouseButtonDown(int button) const override;
		bool IsMouseButtonUp(int button) const override;
		bool IsMouseButtonHeld(int button) const override;
		Vector2 GetMousePosition() const override;
		Vector2 GetMouseDelta() const override;

		bool OnSDLEvent(SDL_Event* event);

	private:
		void InitGamepads();
		void RegisterGamepad(SDL_JoystickID gp);
		void CloseGamepads();
		void CloseGamepad(int id);

		std::unordered_map<int, SDL_Gamepad*> _gamepads = {};

		std::unordered_map<int, std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>> _currGamepadBtnState;
		std::unordered_map<int, std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>> _prevGamepadBtnState;

		std::array<Uint8, SDL_SCANCODE_COUNT> _currKeyState{};
		std::array<Uint8, SDL_SCANCODE_COUNT> _prevKeyState{};

		Uint32 _currMouseState = 0;
		Uint32 _prevMouseState = 0;
		Vector2 _mouseDelta = { 0, 0 };
		Vector2 _mousePos = { 0, 0 };
	};

	TBX_REGISTER_PLUGIN(SDLInputHandlerPlugin);
}
