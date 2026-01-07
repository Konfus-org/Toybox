#include "tbx/common/string_utils.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    std::string TrimString(std::string_view value)
    {
        std::size_t start = 0;
        std::size_t end = value.size();

        while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0)
        {
            ++start;
        }

        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return std::string(value.substr(start, end - start));
    }

    std::string RemoveWhitespace(std::string_view value)
    {
        std::string result = {};
        result.reserve(value.size());

        for (char character : value)
        {
            if (std::isspace(static_cast<unsigned char>(character)) == 0)
            {
                result.push_back(character);
            }
        }

        return result;
    }

    std::string ToLower(std::string_view value)
    {
        std::string result = std::string(value);

        std::transform(
            result.begin(),
            result.end(),
            result.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

        return result;
    }

    std::string ToUpper(std::string_view value)
    {
        std::string result = std::string(value);

        std::transform(
            result.begin(),
            result.end(),
            result.begin(),
            [](unsigned char character) { return static_cast<char>(std::toupper(character)); });

        return result;
    }

    std::string ReplaceAll(
        std::string_view value,
        std::string_view target,
        std::string_view replacement)
    {
        if (target.empty())
        {
            return std::string(value);
        }

        std::string result = {};
        std::size_t search_start = 0;

        while (search_start < value.size())
        {
            std::size_t found = value.find(target, search_start);
            if (found == std::string_view::npos)
            {
                result.append(value.substr(search_start));
                break;
            }

            result.append(value.substr(search_start, found - search_start));
            result.append(replacement);
            search_start = found + target.size();
        }

        return result;
    }

    std::string ReplaceCharacters(
        std::string_view value,
        std::span<const char> characters,
        char replacement)
    {
        std::string result = {};
        result.reserve(value.size());

        for (char character : value)
        {
            if (std::find(characters.begin(), characters.end(), character) != characters.end())
            {
                result.push_back(replacement);
            }
            else
            {
                result.push_back(character);
            }
        }

        return result;
    }

    std::string RemoveAll(std::string_view value, std::string_view target)
    {
        return ReplaceAll(value, target, "");
    }

    std::string RemoveCharacter(std::string_view value, char target)
    {
        std::string result = {};
        result.reserve(value.size());

        for (char character : value)
        {
            if (character != target)
            {
                result.push_back(character);
            }
        }

        return result;
    }

    std::string RemoveCharacters(std::string_view value, std::span<const char> characters)
    {
        std::string result = {};
        result.reserve(value.size());

        for (char character : value)
        {
            if (std::find(characters.begin(), characters.end(), character) == characters.end())
            {
                result.push_back(character);
            }
        }

        return result;
    }
}
