#include "tbx/systems/app/command_list.h"
#include "tbx/utils/string_utils.h"
#include <string_view>

namespace tbx
{
    static bool is_option_token(std::string_view argument)
    {
        return argument.rfind("--", 0) == 0;
    }

    static std::string normalize_option_name(std::string_view argument)
    {
        return std::string(argument.substr(2));
    }

    CommandList::CommandList(int argc, char* argv[])
    {
        for (int index = 1; index < argc; ++index)
        {
            const auto argument =
                std::string_view(argv[index] == nullptr ? "" : argv[index]);
            if (!is_option_token(argument))
            {
                _positionals.emplace_back(argument);
                continue;
            }

            const size_t separator = argument.find('=');
            if (separator != std::string_view::npos)
            {
                _args[std::string(argument.substr(2, separator - 2))] =
                    trim(argument.substr(separator + 1));
                continue;
            }

            if ((index + 1) < argc)
            {
                const auto next_argument =
                    std::string_view(argv[index + 1] == nullptr ? "" : argv[index + 1]);
                if (!next_argument.empty() && !is_option_token(next_argument))
                {
                    _args[normalize_option_name(argument)] = trim(next_argument);
                    ++index;
                    continue;
                }
            }

            _args[normalize_option_name(argument)] = "true";
        }
    }

    bool CommandList::has(const std::string& option) const
    {
        return _args.contains(option);
    }

    const std::vector<std::string>& CommandList::get_positionals() const
    {
        return _positionals;
    }
}
