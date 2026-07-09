#pragma once
#include "tbx/systems/input/action.h"
#include "tbx/systems/input/scheme.generated.h"
#include <initializer_list>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Groups actions into reusable/activatable control schemes.
    /// @details
    /// Ownership: Owns stored actions.
    /// Thread Safety: Not thread-safe; synchronize external access.
    /// Serialization: The scheme's name, initial active state, and actions round-trip, so schemes
    /// can ship inside InputMap assets and load ready to evaluate.
    [[serializable]];
    class TBX_API InputScheme
    {
      public:
        InputScheme() = default;
        explicit InputScheme(std::string scheme_name);
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
        TBX_EXPOSE_PRIVATES_TO_SERIALIZATION;

        [[serialize]]
        std::string _name = {};
        [[serialize]]
        bool _is_active = false;
        // Actions are stored in declaration order (not keyed) so serialized schemes stay stable
        // and editors can present them in authored order; lookups are by name over a small list.
        [[serialize]]
        std::vector<InputAction> _actions = {};
    };
}
