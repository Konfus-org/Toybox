#pragma once
#include "tbx/core/math.h"
#include "tbx/core/typedefs.h"
#include "tbx/platform/keys.h"
#include <array>

namespace tbx
{
    /// @brief
    /// Purpose: Polled input state — gameplay reads this each frame; the KeyEvent signal exists
    /// only for text/UI-style consumers.
    /// @details
    /// Ownership: Owned by Engine. Thread Safety: Main thread only; the platform backend feeds
    /// state during the pump.
    class Input final
    {
      public:
        /// @brief
        /// Purpose: Backend feed: records a key transition.
        void feed_key(Key key, bool down)
        {
            _keys[index(key)] = down;
        }

        /// @brief
        /// Purpose: Backend feed: records a mouse button transition.
        void feed_mouse_button(MouseButton button, bool down)
        {
            _mouse[index(button)] = down;
        }

        /// @brief
        /// Purpose: Backend feed: records the pointer position and accumulates frame delta.
        void feed_mouse_move(Vec2 position, Vec2 delta)
        {
            _mouse_position = position;
            _mouse_delta += delta;
        }

        /// @brief
        /// Purpose: Backend feed: accumulates scroll wheel movement for the frame.
        void feed_scroll(float delta)
        {
            _scroll_delta += delta;
        }

        /// @brief
        /// Purpose: True while the key is held.
        bool is_down(Key key) const
        {
            return _keys[index(key)];
        }

        /// @brief
        /// Purpose: Pointer movement accumulated this frame.
        Vec2 get_mouse_delta() const
        {
            return _mouse_delta;
        }

        /// @brief
        /// Purpose: True while the mouse button is held.
        bool is_mouse_down(MouseButton button) const
        {
            return _mouse[index(button)];
        }

        /// @brief
        /// Purpose: Pointer position in window pixels.
        Vec2 get_mouse_position() const
        {
            return _mouse_position;
        }

        /// @brief
        /// Purpose: True only on the frame the mouse button went down.
        bool is_mouse_pressed(MouseButton button) const
        {
            return _mouse[index(button)] && !_previous_mouse[index(button)];
        }

        /// @brief
        /// Purpose: True only on the frame the mouse button went up.
        bool is_mouse_released(MouseButton button) const
        {
            return !_mouse[index(button)] && _previous_mouse[index(button)];
        }

        /// @brief
        /// Purpose: Rolls per-frame state; called by Engine::pump() before OS events feed in.
        void pump()
        {
            _previous_keys = _keys;
            _previous_mouse = _mouse;
            _mouse_delta = Vec2(0.0f, 0.0f);
            _scroll_delta = 0.0f;
        }

        /// @brief
        /// Purpose: True only on the frame the key went down.
        bool is_pressed(Key key) const
        {
            return _keys[index(key)] && !_previous_keys[index(key)];
        }

        /// @brief
        /// Purpose: True only on the frame the key went up.
        bool is_released(Key key) const
        {
            return !_keys[index(key)] && _previous_keys[index(key)];
        }

        /// @brief
        /// Purpose: Scroll wheel movement accumulated this frame.
        float get_scroll_delta() const
        {
            return _scroll_delta;
        }

      private:
        static size index(Key key)
        {
            return static_cast<size>(key);
        }

        static size index(MouseButton button)
        {
            return static_cast<size>(button);
        }

      private:
        std::array<bool, static_cast<size>(Key::COUNT)> _keys = {};
        std::array<bool, static_cast<size>(Key::COUNT)> _previous_keys = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> _mouse = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> _previous_mouse = {};
        Vec2 _mouse_position = Vec2(0.0f, 0.0f);
        Vec2 _mouse_delta = Vec2(0.0f, 0.0f);
        float _scroll_delta = 0.0f;
    };
}
