#pragma once
#include "tbx/systems/math/vectors.h"
#include "tbx/systems/time/delta_time.h"
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Provides strongly-typed key identifiers for keyboard action bindings.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputKey : int
    {
        UNKNOWN = 0,
        A = 4,
        B = 5,
        C = 6,
        D = 7,
        E = 8,
        F = 9,
        G = 10,
        H = 11,
        I = 12,
        J = 13,
        K = 14,
        L = 15,
        M = 16,
        N = 17,
        O = 18,
        P = 19,
        Q = 20,
        R = 21,
        S = 22,
        T = 23,
        U = 24,
        V = 25,
        W = 26,
        X = 27,
        Y = 28,
        Z = 29,
        ALPHA_1 = 30,
        ALPHA_2 = 31,
        ALPHA_3 = 32,
        ALPHA_4 = 33,
        ALPHA_5 = 34,
        ALPHA_6 = 35,
        ALPHA_7 = 36,
        ALPHA_8 = 37,
        ALPHA_9 = 38,
        ALPHA_0 = 39,
        RETURN = 40,
        ESCAPE = 41,
        BACKSPACE = 42,
        TAB = 43,
        SPACE = 44,
        MINUS = 45,
        EQUALS = 46,
        LEFT_BRACKET = 47,
        RIGHT_BRACKET = 48,
        BACKSLASH = 49,
        NONUSHASH = 50,
        SEMICOLON = 51,
        APOSTROPHE = 52,
        GRAVE = 53,
        COMMA = 54,
        PERIOD = 55,
        SLASH = 56,
        CAPSLOCK = 57,
        F1 = 58,
        F2 = 59,
        F3 = 60,
        F4 = 61,
        F5 = 62,
        F6 = 63,
        F7 = 64,
        F8 = 65,
        F9 = 66,
        F10 = 67,
        F11 = 68,
        F12 = 69,
        PRINT_SCREEN = 70,
        SCROLL_LOCK = 71,
        PAUSE = 72,
        INSERT = 73,
        HOME = 74,
        PAGEUP = 75,
        DELETE = 76,
        END = 77,
        PAGEDOWN = 78,
        RIGHT = 79,
        LEFT = 80,
        DOWN = 81,
        UP = 82,
        NUM_LOCK_CLEAR = 83,
        KP_DIVIDE = 84,
        KP_MULTIPLY = 85,
        KP_MINUS = 86,
        KP_PLUS = 87,
        KP_ENTER = 88,
        KP_1 = 89,
        KP_2 = 90,
        KP_3 = 91,
        KP_4 = 92,
        KP_5 = 93,
        KP_6 = 94,
        KP_7 = 95,
        KP_8 = 96,
        KP_9 = 97,
        KP_0 = 98,
        KP_PERIOD = 99,
        NONUS_BACKSLASH = 100,
        APPLICATION = 101,
        POWER = 102,
        KP_EQUALS = 103,
        F13 = 104,
        F14 = 105,
        F15 = 106,
        F16 = 107,
        F17 = 108,
        F18 = 109,
        F19 = 110,
        F20 = 111,
        F21 = 112,
        F22 = 113,
        F23 = 114,
        F24 = 115,
        EXECUTE = 116,
        HELP = 117,
        MENU = 118,
        SELECT = 119,
        STOP = 120,
        AGAIN = 121,
        UNDO = 122,
        CUT = 123,
        COPY = 124,
        PASTE = 125,
        FIND = 126,
        MUTE = 127,
        VOLUME_UP = 128,
        VOLUME_DOWN = 129,
        LOCKING_CAPSLOCK = 130,
        LOCKING_NUMLOCK = 131,
        LOCKING_SCROLLLOCK = 132,
        KP_COMMA = 133,
        KP_EQUAL_SAS400 = 134,
        INTERNATIONAL1 = 135,
        INTERNATIONAL2 = 136,
        INTERNATIONAL3 = 137,
        INTERNATIONAL4 = 138,
        INTERNATIONAL5 = 139,
        INTERNATIONAL6 = 140,
        INTERNATIONAL7 = 141,
        INTERNATIONAL8 = 142,
        INTERNATIONAL9 = 143,
        LANG1 = 144,
        LANG2 = 145,
        LANG3 = 146,
        LANG4 = 147,
        LANG5 = 148,
        LANG6 = 149,
        LANG7 = 150,
        LANG8 = 151,
        LANG9 = 152,
        ALTERASE = 153,
        SYSREQ = 154,
        CANCEL = 155,
        CLEAR = 156,
        PRIOR = 157,
        RETURN2 = 158,
        SEPARATOR = 159,
        OUT = 160,
        OPER = 161,
        CLEARAGAIN = 162,
        CRSEL = 163,
        EXSEL = 164,
        KP_00 = 176,
        KP_000 = 177,
        THOUSANDSSEPARATOR = 178,
        DECIMALSEPARATOR = 179,
        CURRENCYUNIT = 180,
        CURRENCYSUBUNIT = 181,
        KP_LEFTPAREN = 182,
        KP_RIGHTPAREN = 183,
        KP_LEFTBRACE = 184,
        KP_RIGHTBRACE = 185,
        KP_TAB = 186,
        KP_BACKSPACE = 187,
        KP_A = 188,
        KP_B = 189,
        KP_C = 190,
        KP_D = 191,
        KP_E = 192,
        KP_F = 193,
        KP_XOR = 194,
        KP_POWER = 195,
        KP_PERCENT = 196,
        KP_LESS = 197,
        KP_GREATER = 198,
        KP_AMPERSAND = 199,
        KP_DBLAMPERSAND = 200,
        KP_VERTICALBAR = 201,
        KP_DBLVERTICALBAR = 202,
        KP_COLON = 203,
        KP_HASH = 204,
        KP_SPACE = 205,
        KP_AT = 206,
        KP_EXCLAM = 207,
        KP_MEMSTORE = 208,
        KP_MEMRECALL = 209,
        KP_MEMCLEAR = 210,
        KP_MEMADD = 211,
        KP_MEMSUBTRACT = 212,
        KP_MEMMULTIPLY = 213,
        KP_MEMDIVIDE = 214,
        KP_PLUSMINUS = 215,
        KP_CLEAR = 216,
        KP_CLEARENTRY = 217,
        KP_BINARY = 218,
        KP_OCTAL = 219,
        KP_DECIMAL = 220,
        KP_HEXADECIMAL = 221,
        LCTRL = 224,
        LSHIFT = 225,
        LALT = 226,
        LGUI = 227,
        RCTRL = 228,
        RSHIFT = 229,
        RALT = 230,
        RGUI = 231,
        MODE = 257,
        SLEEP = 258,
        WAKE = 259,
        CHANNEL_INCREMENT = 260,
        CHANNEL_DECREMENT = 261,
        MEDIA_PLAY = 262,
        MEDIA_PAUSE = 263,
        MEDIA_RECORD = 264,
        MEDIA_FAST_FORWARD = 265,
        MEDIA_REWIND = 266,
        MEDIA_NEXT_TRACK = 267,
        MEDIA_PREVIOUS_TRACK = 268,
        MEDIA_STOP = 269,
        MEDIA_EJECT = 270,
        MEDIA_PLAY_PAUSE = 271,
        MEDIA_SELECT = 272,
        AC_NEW = 273,
        AC_OPEN = 274,
        AC_CLOSE = 275,
        AC_EXIT = 276,
        AC_SAVE = 277,
        AC_PRINT = 278,
        AC_PROPERTIES = 279,
        AC_SEARCH = 280,
        AC_HOME = 281,
        AC_BACK = 282,
        AC_FORWARD = 283,
        AC_STOP = 284,
        AC_REFRESH = 285,
        AC_BOOKMARKS = 286,
        SOFTLEFT = 287,
        SOFTRIGHT = 288,
        CALL = 289,
        ENDCALL = 290,
        RESERVED = 400,
        COUNT = 512,
    };

    /// @brief
    /// Purpose: Provides strongly-typed mouse button identifiers for bindings.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputMouseButton : int
    {
        UNKNOWN = 0,
        LEFT = 1,
        MIDDLE = 2,
        RIGHT = 3,
        X1 = 4,
        X2 = 5,
    };

    /// @brief
    /// Purpose: Provides strongly-typed gamepad button identifiers for bindings.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputControllerButton : int
    {
        UNKNOWN = -1,
        SOUTH = 0,
        EAST = 1,
        WEST = 2,
        NORTH = 3,
        BACK = 4,
        GUIDE = 5,
        START = 6,
        LEFT_STICK = 7,
        RIGHT_STICK = 8,
        LEFT_SHOULDER = 9,
        RIGHT_SHOULDER = 10,
        DPAD_UP = 11,
        DPAD_DOWN = 12,
        DPAD_LEFT = 13,
        DPAD_RIGHT = 14,
    };

    /// @brief
    /// Purpose: Provides strongly-typed gamepad axis identifiers for axis/vector bindings.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputControllerAxis : int
    {
        UNKNOWN = -1,
        LEFT_X = 0,
        LEFT_Y = 1,
        RIGHT_X = 2,
        RIGHT_Y = 3,
        LEFT_TRIGGER = 4,
        RIGHT_TRIGGER = 5,
    };

    /// @brief
    /// Purpose: Selects a vector-producing mouse control.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputMouseVectorControl
    {
        POSITION,
        DELTA,
    };

    /// @brief
    /// Purpose: Selects a float-producing mouse control.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class InputMouseAxisControl
    {
        WHEEL,
    };

    /// @brief
    /// Purpose: Associates an action with a specific keyboard key.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API KeyboardInputControl
    {
        InputKey key = InputKey::UNKNOWN;
    };

    /// @brief
    /// Purpose: Associates an action with a specific mouse button.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API MouseButtonInputControl
    {
        InputMouseButton button = InputMouseButton::UNKNOWN;
    };

    /// @brief
    /// Purpose: Associates an action with a vector mouse control (position or delta).
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API MouseVectorInputControl
    {
        InputMouseVectorControl control = InputMouseVectorControl::DELTA;
    };

    /// @brief
    /// Purpose: Associates an action with a float mouse control (wheel).
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API MouseAxisInputControl
    {
        InputMouseAxisControl control = InputMouseAxisControl::WHEEL;
    };

    /// @brief
    /// Purpose: Associates an action with a specific button on a target controller.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API ControllerButtonInputControl
    {
        int controller_index = -1;
        InputControllerButton button = InputControllerButton::UNKNOWN;
    };

    /// @brief
    /// Purpose: Associates an action with a specific axis on a target controller.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API ControllerAxisInputControl
    {
        int controller_index = -1;
        InputControllerAxis axis = InputControllerAxis::UNKNOWN;
    };

    /// @brief
    /// Purpose: Associates an action with two axes on a target controller.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API ControllerStickInputControl
    {
        int controller_index = -1;
        InputControllerAxis x_axis = InputControllerAxis::UNKNOWN;
        InputControllerAxis y_axis = InputControllerAxis::UNKNOWN;
    };

    /// @brief
    /// Purpose: Maps four directional keys into one Vector2 action value.
    /// @details
    /// Ownership: Value type with no dynamic ownership.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    struct TBX_API KeyboardVector2CompositeInputControl
    {
        InputKey up = InputKey::UNKNOWN;
        InputKey down = InputKey::UNKNOWN;
        InputKey left = InputKey::UNKNOWN;
        InputKey right = InputKey::UNKNOWN;
    };

    /// @brief
    /// Purpose: Unifies strongly-typed keyboard/mouse/controller controls in one API.
    /// @details
    /// Ownership: Value type; callers own copied values.
    /// Thread Safety: Safe for concurrent read access to immutable instances.
    using InputControl = std::variant<
        KeyboardInputControl,
        MouseButtonInputControl,
        MouseVectorInputControl,
        MouseAxisInputControl,
        KeyboardVector2CompositeInputControl,
        ControllerButtonInputControl,
        ControllerAxisInputControl,
        ControllerStickInputControl>;

    /// @brief
    /// Purpose: Defines what physical control drives the action and how it is scaled.
    /// @details
    /// Ownership: Value type; owns copied control values.
    /// Thread Safety: Not thread-safe for concurrent mutation.
    struct TBX_API InputBinding
    {
        /// @brief
        /// Purpose: Identifies the keyboard/mouse/controller source for the action.
        /// @details
        /// Ownership: Value type stored by this binding.
        /// Thread Safety: Safe for concurrent reads of immutable bindings.
        InputControl control = KeyboardInputControl {};

        /// @brief
        /// Purpose: Supports sensitivity/inversion/tuning per binding.
        /// @details
        /// Ownership: Primitive value stored by this binding.
        /// Thread Safety: Safe for concurrent reads of immutable bindings.
        float scale = 1.0F;
    };

    /// @brief
    /// Purpose: Unifies button, axis, and vector controls under one API.
    /// @details
    /// Ownership: Value type; callers own copies.
    /// Thread Safety: Safe for concurrent read access to independent copies.
    using InputActionValue = std::variant<bool, float, Vec2>;

    class InputAction;

    /// @brief Represents the callback signature for input action lifecycle notifications.
    /// @details Purpose: Used by on-start/on-performed/on-cancelled
    /// subscriptions. Ownership: Callable value copied by action instances. Thread Safety:
    /// Invocation occurs on the input update thread.
    using InputActionCallback = std::function<void(const InputAction&)>;

    /// @brief
    /// Purpose: Enables one-shot action construction with predefined bindings and subscriptions.
    /// @details
    /// Ownership: Owns binding and callback collections by value.
    /// Thread Safety: Not thread-safe for mutation; synchronize externally if shared.
    struct TBX_API InputActionConstruction
    {
        std::vector<InputBinding> bindings = {};
        std::vector<InputActionCallback> on_start_callbacks = {};
        std::vector<InputActionCallback> on_performed_callbacks = {};
        std::vector<InputActionCallback> on_cancelled_callbacks = {};
    };

    /// @brief
    /// Purpose: Helps map action bindings to stable gameplay value shapes.
    /// @details
    /// Ownership: Enum value type.
    /// Thread Safety: Safe for concurrent use.
    enum class InputActionValueType
    {
        BUTTON,
        AXIS,
        VECTOR2
    };

    /// @brief
    /// Purpose: Defines gameplay actions and lifecycle callbacks.
    /// @details
    /// Ownership: Owns bindings and callback lists.
    /// Thread Safety: Not thread-safe; mutate/query from a synchronized context.
    class TBX_API InputAction
    {
      public:
        InputAction(std::string action_name, InputActionValueType value_type);
        InputAction(
            std::string action_name,
            InputActionValueType value_type,
            InputActionConstruction construction);

        const std::string& get_name() const;
        InputActionValueType get_value_type() const;
        const std::vector<InputBinding>& get_bindings() const;
        const InputActionValue& get_value() const;

        /// @brief
        /// Purpose: Simplifies typed value reads without repetitive variant checks.
        /// @details
        /// Ownership: Writes a copied value into the provided output reference.
        /// Thread Safety: Read-only operation; synchronize externally if shared mutably.
        template <typename TValue>
        bool try_get_value_as(TValue& out_value) const;

        /// @brief
        /// Purpose: Provides optional typed access without copying when available.
        /// @details
        /// Ownership: Returns a non-owning reference that aliases internal action storage.
        /// Thread Safety: Read-only operation; synchronize externally if shared mutably.
        template <typename TValue>
        std::optional<std::reference_wrapper<const TValue>> try_get_value_as() const;

        bool get_is_active() const;
        std::chrono::duration<double> get_held_time() const;

        void add_binding(const InputBinding& binding);
        bool remove_binding(const InputBinding& binding);

        void add_on_start_callback(InputActionCallback callback);
        void add_on_performed_callback(InputActionCallback callback);
        void add_on_cancelled_callback(InputActionCallback callback);

        void apply_value(const InputActionValue& value, const DeltaTime& delta_time);

      private:
        void invoke_on_start() const;
        void invoke_on_performed() const;
        void invoke_on_cancelled() const;

        std::string _name = {};
        InputActionValueType _value_type = InputActionValueType::BUTTON;
        std::vector<InputBinding> _bindings = {};
        InputActionValue _value = false;
        bool _is_active = false;
        std::chrono::duration<double> _held_time = {};
        std::vector<InputActionCallback> _on_start_callbacks = {};
        std::vector<InputActionCallback> _on_performed_callbacks = {};
        std::vector<InputActionCallback> _on_cancelled_callbacks = {};
    };
}

#include "tbx/systems/input/action.inl"
