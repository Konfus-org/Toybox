#pragma once
#include "tbx/core/tbx_api.h"
#include <span>
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Trims leading and trailing whitespace from the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy of the trimmed text.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string trim(std::string_view value);

    /// @brief
    /// Purpose: Removes all whitespace characters from the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy without whitespace characters.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string remove_whitespace(std::string_view value);

    /// @brief
    /// Purpose: Returns a lowercase copy of the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy in lowercase form.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string to_lower(std::string_view value);

    /// @brief
    /// Purpose: Returns true when the provided token appears within the text, ignoring case.
    /// @details
    /// Ownership: Does not transfer ownership.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API bool contains_case_insensitive(std::string_view value, std::string_view token);

    /// @brief
    /// Purpose: Returns an uppercase copy of the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy in uppercase form.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string to_upper(std::string_view value);

    /// @brief
    /// Purpose: Replaces all occurrences of the target substring in the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy with replacements applied.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string replace(
        std::string_view value,
        std::string_view target,
        std::string_view replacement);

    /// @brief
    /// Purpose: Replaces any occurrence of characters from the provided list with a replacement
    /// character.
    /// @details
    /// Ownership: Returns an owned std::string copy with character replacements applied.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string replace(
        std::string_view value,
        std::span<const char> characters,
        char replacement);

    /// @brief
    /// Purpose: Removes all occurrences of the target substring from the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy with target text removed.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string remove(std::string_view value, std::string_view target);

    /// @brief
    /// Purpose: Removes all occurrences of the target character from the provided text.
    /// @details
    /// Ownership: Returns an owned std::string copy with target characters removed.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string remove(std::string_view value, char target);

    /// @brief
    /// Purpose: Removes all occurrences of characters from the provided list.
    /// @details
    /// Ownership: Returns an owned std::string copy with target characters removed.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API std::string remove(std::string_view value, std::span<const char> characters);
}
