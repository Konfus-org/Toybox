#include "tbx/utils/command_list.h"

namespace tbx
{

    namespace internal
    {
        static size option_name_start(const std::string_view argument)
        {
            return argument.starts_with("--") ? 2 : 1;
        }

        static bool is_option_token(const std::string_view argument)
        {
            if (!argument.starts_with('-'))
                return false;
            const size start = option_name_start(argument);
            if (start >= argument.size())
                return false;
            // "-5" / "-.5" are negative-number values or positionals, never options.
            const char first = argument[start];
            return (first < '0' || first > '9') && first != '.';
        }

    }
    //// HELPERS ////



    //// COMMAND LIST ////

    CommandList::CommandList(const int argc, char** argv)
    {
        for (int index = 1; index < argc; ++index)
        {
            const auto argument = std::string_view(argv[index] ? argv[index] : "");
            if (!internal::is_option_token(argument))
            {
                _positionals.emplace_back(argument);
                continue;
            }
            const auto name_and_value = argument.substr(internal::option_name_start(argument));

            // "--name=value" binds inside one token.
            const size separator = name_and_value.find('=');
            if (separator != std::string_view::npos)
            {
                _options[std::string(name_and_value.substr(0, separator))] =
                    trim(name_and_value.substr(separator + 1));
                continue;
            }

            // "--name value" consumes the next token unless it is another option.
            if (index + 1 < argc)
            {
                const auto next = std::string_view(argv[index + 1] ? argv[index + 1] : "");
                if (!next.empty() && !internal::is_option_token(next))
                {
                    _options[std::string(name_and_value)] = trim(next);
                    ++index;
                    continue;
                }
            }

            // A bare "--flag" reads back as true.
            _options[std::string(name_and_value)] = "true";
        }
    }

    const std::vector<std::string>& CommandList::get_positionals() const
    {
        return _positionals;
    }

    bool CommandList::has(const std::string_view option) const
    {
        return _options.contains(option);
    }

    std::string CommandList::to_string() const
    {
        auto stream = std::ostringstream();
        bool is_first = true;
        const auto append = [&](const std::string_view token)
        {
            if (!is_first)
                stream << ' ';
            stream << token;
            is_first = false;
        };
        for (const std::string& positional : _positionals)
            append(positional);
        for (const auto& [name, value] : _options)
            append("--" + name + "=" + value);
        return stream.str();
    }

    std::string CommandList::trim(const std::string_view text)
    {
        constexpr std::string_view WHITESPACE = " \t\r\n";
        const size first = text.find_first_not_of(WHITESPACE);
        if (first == std::string_view::npos)
            return {};
        const size last = text.find_last_not_of(WHITESPACE);
        return std::string(text.substr(first, last - first + 1));
    }
}
