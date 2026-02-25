#pragma once
#include "tbx/input/input_action.h"
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// <summary>Represents a named collection of input actions.</summary>
    /// <remarks>Purpose: Groups actions into reusable/activatable control schemes.
    /// Ownership: Owns stored actions.
    /// Thread Safety: Not thread-safe; synchronize external access.</remarks>
    class TBX_API InputScheme
    {
      public:
        /// <summary>Constructs an empty named input scheme.</summary>
        /// <remarks>Purpose: Creates a scheme for actions added later.
        /// Ownership: Stores the scheme name by value and owns any later-added actions.
        /// Thread Safety: Construction is thread-confined; synchronize shared access
        /// after.</remarks>
        InputScheme(std::string scheme_name);
        /// <summary>Constructs a named input scheme with an initializer-list of actions.</summary>
        /// <remarks>Purpose: Enables concise inline scheme declarations with predefined actions.
        /// Ownership: Copies provided actions into scheme-owned storage.
        /// Thread Safety: Construction is thread-confined; synchronize shared access
        /// after.</remarks>
        InputScheme(std::string scheme_name, std::initializer_list<InputAction> actions);
        /// <summary>Constructs a named input scheme with a vector of actions.</summary>
        /// <remarks>Purpose: Supports bulk action setup from pre-built action collections.
        /// Ownership: Copies provided actions into scheme-owned storage.
        /// Thread Safety: Construction is thread-confined; synchronize shared access
        /// after.</remarks>
        InputScheme(std::string scheme_name, std::vector<InputAction> actions);

        const std::string& get_name() const;
        bool get_is_active() const;
        void set_is_active(bool is_active);

        bool add_action(const InputAction& action);
        bool remove_action(const std::string& action_name);
        InputAction* try_get_action(const std::string& action_name);
        const InputAction* try_get_action(const std::string& action_name) const;
        std::vector<std::reference_wrapper<InputAction>> get_all_actions();
        std::vector<std::reference_wrapper<const InputAction>> get_all_actions() const;

      private:
        std::string _name = {};
        bool _is_active = false;
        std::unordered_map<std::string, InputAction> _actions = {};
    };
}
