#include "tbx/examples/input_controller.h"
#include "tbx/debugging/macros.h"

namespace examples_common
{
    void InputController::bind_input_context(
        InputManager& input_manager,
        const std::string& scheme_name)
    {
        _input_manager = &input_manager;
        _scheme_name = scheme_name;
    }

    void InputController::clear_input_context()
    {
        _input_manager = nullptr;
        _scheme_name.clear();
    }

    const std::string& InputController::get_input_scheme_name() const
    {
        return _scheme_name;
    }

    tbx::InputScheme& InputController::get_registered_input_scheme() const
    {
        static auto fallback_scheme = tbx::InputScheme("");
        TBX_ASSERT(
            _input_manager != nullptr,
            "InputController has no InputManager bound for scheme '{}'.",
            _scheme_name);
        if (_input_manager == nullptr)
            return fallback_scheme;

        auto* input_scheme = _input_manager->get_scheme(_scheme_name);
        TBX_ASSERT(
            input_scheme != nullptr,
            "InputController could not resolve input scheme '{}' in InputManager.",
            _scheme_name);
        if (input_scheme == nullptr)
            return fallback_scheme;

        return *input_scheme;
    }
}
