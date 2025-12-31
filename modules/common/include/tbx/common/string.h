#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cassert>
#include <format>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tbx
{
    // Returns a copy of this string without leading or trailing whitespace characters.
    std::string trim(std::string s) const;

    // Returns a copy of this string with all whitespace characters removed.
    std::string remove_whitespace(std::string s) const;

    // Returns a lowercase copy of this string.
    std::string to_lower(std::string s) const;

    // Returns an uppercase copy of this string.
    std::string to_upper(std::string s) const;

    // Returns a copy where every instance of the provided character is replaced.
    std::string replace(std::string s, char target, char replacement) const;

    // Returns a copy where every occurrence of the target substring is replaced.
    std::string replace(std::string s, const std::string& target, const std::string& replacement)
        const;

    // Returns a copy where every member of the provided character array is replaced.
    std::string replace(std::string s, const List<char>& characters, char replacement) const;

    // True if the string starts with the provided prefix.
    bool starts_with(std::string s, const std::string& prefix) const;

    // True if the string ends with the provided suffix.
    bool ends_with(std::string s, const std::string& suffix) const;

    // True if the string contains the provided substring.
    bool contains(std::string s, const std::string& substr) const;

    // True if the string is empty.
    bool empty(std::string s) const;

    // Removes a substring starting at the provided position.
    std::string& remove(std::string s, size_t position, size_t count = std::string::npos);

    // Returns a copy with the provided character removed.
    std::string remove(std::string s, char target) const;

    // Returns a copy with every occurrence of the target substring removed.
    std::string remove(std::string s, const std::string& target) const;

    // Returns a copy with every character from the provided array removed.
    std::string remove(std::string s, const std::vector<char>& characters) const;
}
