#pragma once
#include "tbx/systems/input/action.h"
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Groups actions into reusable/activatable control schemes.
    /// @details
    /// Ownership: Owns stored actions.
    /// Thread Safety: Not thread-safe; synchronize external access.
    class TBX_API InputScheme
    {
      public:
        InputScheme(std::string scheme_name);
        InputScheme(std::string scheme_name, std::initializer_list<InputAction> actions);
        InputScheme(std::string scheme_name, std::vector<InputAction> actions);

        const std::string& get_name() const;
        bool get_is_active() const;
        void set_is_active(bool is_active);

        bool add_action(const InputAction& action);
        bool remove_action(const std::string& action_name);
        std::optional<std::reference_wrapper<InputAction>> try_get_action(
            const std::string& action_name);
        std::optional<std::reference_wrapper<const InputAction>> try_get_action(
            const std::string& action_name) const;
        std::vector<std::reference_wrapper<InputAction>> get_all_actions();
        std::vector<std::reference_wrapper<const InputAction>> get_all_actions() const;

      private:
        std::string _name = {};
        bool _is_active = false;
        std::unordered_map<std::string, InputAction> _actions = {};
    };
}
