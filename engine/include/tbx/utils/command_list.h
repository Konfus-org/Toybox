#pragma once
#include "tbx/api.h"
#include "tbx/utils/typedefs.h"
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Parsed command line: "--name=value" / "--name value" / "-name value" options
    /// plus positional arguments (a leading digit is a negative number, never an option).
    /// Flags without a value read back as "true" ("--selftest" -> get<bool>("selftest") ==
    /// true). Parse once at startup, read anywhere.
    class TBX_API CommandList final
    {
      public:
        CommandList() = default;
        // C boundary: argc/argv arrive exactly as main() receives them.
        CommandList(int argc, char** argv);

      public:
        /// @brief
        /// Purpose: The named option converted to the requested type; a missing option or a
        /// value that does not parse returns the default.
        template <typename TValue>
        TValue get(const std::string_view option, TValue default_value = TValue()) const
        {
            const auto found = _options.find(option);
            if (found == _options.end())
                return default_value;
            if constexpr (std::is_same_v<TValue, std::string>)
                return found->second;
            else
            {
                auto stream = std::stringstream(found->second);
                stream >> std::boolalpha;
                auto result = TValue();
                if (stream >> result)
                    return result;
                return default_value;
            }
        }

        /// @brief
        /// Purpose: A comma-separated option parsed into a list; entries that do not parse
        /// are skipped, a missing option is an empty list.
        template <typename TValue>
        std::vector<TValue> get_list(const std::string_view option) const
        {
            const auto found = _options.find(option);
            if (found == _options.end())
                return {};
            auto results = std::vector<TValue>();
            auto stream = std::stringstream(found->second);
            auto item = std::string();
            while (std::getline(stream, item, ','))
            {
                if constexpr (std::is_same_v<TValue, std::string>)
                {
                    auto value = trim(item);
                    if (!value.empty())
                        results.push_back(std::move(value));
                }
                else
                {
                    auto item_stream = std::stringstream(trim(item));
                    item_stream >> std::boolalpha;
                    auto value = TValue();
                    if (item_stream >> value)
                        results.push_back(value);
                }
            }
            return results;
        }

        /// @brief
        /// Purpose: Positional arguments in their original order.
        const std::vector<std::string>& get_positionals() const;

        /// @brief
        /// Purpose: True when the named option was provided.
        bool has(std::string_view option) const;

        /// @brief
        /// Purpose: The parsed arguments as one diagnostic string — positionals first, then
        /// "--name=value" options in sorted order.
        std::string to_string() const;

      private:
        /// @brief
        /// Purpose: Strips surrounding whitespace from option values and list entries.
        static std::string trim(std::string_view text);

      private:
        // std::less<> enables string_view lookups; the sorted order keeps to_string stable.
        std::map<std::string, std::string, std::less<>> _options = {};
        std::vector<std::string> _positionals = {};
    };
}
