#pragma once
#include "tbx/tbx_api.h"
#include <span>
#include <string>
#include <string_view>

namespace tbx
{
    /// Trims leading and trailing whitespace from the provided text.
    /// Ownership: Returns an owned std::string copy of the trimmed text.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string TrimString(std::string_view value);

    /// Removes all whitespace characters from the provided text.
    /// Ownership: Returns an owned std::string copy without whitespace characters.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string RemoveWhitespace(std::string_view value);

    /// Returns a lowercase copy of the provided text.
    /// Ownership: Returns an owned std::string copy in lowercase form.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string ToLower(std::string_view value);

    /// Returns an uppercase copy of the provided text.
    /// Ownership: Returns an owned std::string copy in uppercase form.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string ToUpper(std::string_view value);

    /// Replaces all occurrences of the target substring in the provided text.
    /// Ownership: Returns an owned std::string copy with replacements applied.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string ReplaceAll(
        std::string_view value,
        std::string_view target,
        std::string_view replacement);

    /// Replaces any occurrence of characters from the provided list with a replacement character.
    /// Ownership: Returns an owned std::string copy with character replacements applied.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string ReplaceCharacters(
        std::string_view value,
        std::span<const char> characters,
        char replacement);

    /// Removes all occurrences of the target substring from the provided text.
    /// Ownership: Returns an owned std::string copy with target text removed.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string RemoveAll(std::string_view value, std::string_view target);

    /// Removes all occurrences of the target character from the provided text.
    /// Ownership: Returns an owned std::string copy with target characters removed.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string RemoveCharacter(std::string_view value, char target);

    /// Removes all occurrences of characters from the provided list.
    /// Ownership: Returns an owned std::string copy with target characters removed.
    /// Thread-safety: Thread-safe; does not mutate shared state.
    TBX_API std::string RemoveCharacters(
        std::string_view value,
        std::span<const char> characters);
}
