#pragma once
#include "tbx/tbx_api.h"
#include "tbx/utils/string_utils.h"
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace tbx
{
    class TBX_API CommandList
    {
      public:
        /// @brief
        /// Purpose: Parses command line arguments into named options and positional arguments.
        /// @details
        /// Ownership: Copies parsed argument values into owned containers.
        /// Thread Safety: Not thread-safe; intended for single-threaded startup.
        CommandList(int argc, char* argv[]);

      public:
        /// @brief
        /// Purpose: Returns true when the named option was provided.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Thread-safe for concurrent reads after construction.
        bool has(const std::string& option) const;

        /// @brief
        /// Purpose: Returns the named option converted to the requested type or a default value.
        /// @details
        /// Ownership: Returns an owned value.
        /// Thread Safety: Thread-safe for concurrent reads after construction.
        template <typename TValue>
        TValue get(const std::string& option, TValue default_value = TValue()) const
        {
            const auto iterator = _args.find(option);
            if (iterator == _args.end())
            {
                return default_value;
            }

            if constexpr (std::is_same_v<TValue, std::string>)
            {
                return iterator->second;
            }
            else
            {
                auto stream = std::stringstream(iterator->second);
                stream >> std::boolalpha;

                auto result = TValue();
                if (stream >> result)
                {
                    return result;
                }

                return default_value;
            }
        }

        /// @brief
        /// Purpose: Returns a comma-separated option parsed into a list of values.
        /// @details
        /// Ownership: Returns an owned vector of parsed values.
        /// Thread Safety: Thread-safe for concurrent reads after construction.
        template <typename TValue>
        std::vector<TValue> get_list(const std::string& option) const
        {
            const auto iterator = _args.find(option);
            if (iterator == _args.end())
            {
                return {};
            }

            auto results = std::vector<TValue>();
            auto stream = std::stringstream(iterator->second);
            auto item = std::string();
            while (std::getline(stream, item, ','))
            {
                if constexpr (std::is_same_v<TValue, std::string>)
                {
                    auto value = trim(item);
                    if (!value.empty())
                    {
                        results.push_back(std::move(value));
                    }
                }
                else
                {
                    auto item_stream = std::stringstream(trim(item));
                    item_stream >> std::boolalpha;

                    auto value = TValue();
                    if (item_stream >> value)
                    {
                        results.push_back(value);
                    }
                }
            }

            return results;
        }

        /// @brief
        /// Purpose: Returns positional command line arguments in their original order.
        /// @details
        /// Ownership: Returns a const reference owned by the command list.
        /// Thread Safety: Thread-safe for concurrent reads after construction.
        const std::vector<std::string>& get_positionals() const;

      private:
        std::map<std::string, std::string> _args = {};
        std::vector<std::string> _positionals = {};
    };
}
